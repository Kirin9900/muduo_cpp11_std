# TcpConnection

TcpConnection是muduo最核心的class,是muduo里面唯一默认使用shared_ptr来管理的class,也是唯一继承enable_shared_from_this的class，这源于其模糊的生命期

目前TcpConnection的状态只有两个，kConnecting和kConnected

TcpConnection使用Channel来获得socket上的IO事件，它会自己处理writable事件，而把readable事件通过MessageCallback传给客户

TcpConnection拥有TCP socket，它的析构函数会close(fd)

~~~C++
private:
  enum StateE{kConnecting,kConnected};

  void setState(StateE s){state_ = s;}
  void handleRead();

  EventLoop* loop_;
  std::string name_;
  StateE state_;
  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;
  InetAddress localAddr_;
  InetAddress peerAddr_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
~~~

注意TCpConnection表示的是“一次TCP连接”，它是不可再生的，一旦连接断开，这个TcpConnection对象就没什么用了，另外TCpConnection没有发起连接的功能，其构造函数是已经建立好连接的socketfd(无论是TcpServer被动接受还是TcpClient主动发起)，因此其初始状态是kConnecting



#### 1. TcpConnection 断开连接

muduo只有一种关闭连接的方式：被动关闭，即对方先关闭连接，本地read返回0，触发关闭逻辑。将来如果有必要也可以给TcpConnection新增forceClose()成员函数，用于主动关闭连接，实现很简单，调用handleClose()即可。

![TcpConnection](/img/img_19.png)


**Channel的改动**

新增了CloseCallback事件回调，并且断言(assert())在事件处理期间本Channel对象不会析构

~~~C++
void muduo::Channel::handleEvent() {
  eventHandling_ = true;
  if(revents_& POLLNVAL) {
    LOG<<"Channel::handel_event() POLLNVAL";
  }
  if(revents_* (POLLERR|POLLNVAL)) {
    if(errorCallback_) errorCallback_();
  }

  //POLLHUP 表示挂起事件，即对方关闭了连接或连接断开。
  if((revents_& POLLHUP)&&!(revents_&POLLIN)) {
    LOG<<"Channel::handle_event() POLLHUP";
    if(closeCallback_) closeCallback_();
  }

  if(revents_&(POLLIN|POLLPRI/POLLRDHUP)) {
    if(readCallback_){return readCallback_();}
  }
  if(revents_&(POLLOUT)) {
    if(writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}
~~~



**TcpConnection的改动**

TcoConnection class 也新增了CloseCallback事件回调，但是这个回调是给TcpServer和TcpClient使用的，用于通知它们移除所持有的TcpConnectionPtr,这不是给普通用户使用的，普通用户继续使用ConnectionCallback

~~~C++
void TcpConnection::handleClose(){
  loop_->assertInLoopThread();
  LOG<<"TcpConnection::handleClose state = "<<state_;
  assert(state_ == kConnected);
  channel_->disableAll();
  closeCallback_(shared_from_this());
}

void TcpConnection::handleError() {
  int err = sockets::getSocketError(channel_->fd());
  LOG<<"TcpConnection::handleError ["<<name_<<"] - SO_ERROR= " << err
      << " " << strerror(err);
}

void TcpConnection::connectDestroyed(){
  loop_->assertInLoopThread();
  assert(state_ == kConnected);
  setState(kDisConnected);
  channel_->disableAll();
  closeCallback_(shared_from_this());
}
~~~



**TcpServer的改动**

TcpServer 向TcpConnection注册CloseCallback,用于接收连接断开的消息

通常TcpServer的生命期长于它建立的TcpConnection，TcpServer的析构函数会关闭连接，因此也是安全的



```C++
void TcpServer::removeConnection(const TcpConnectionPtr& conn){
  loop_->assertInLoopThread();
  LOG<<"TcpServer::removeConnection ["<<name_<<"] - connection"<<conn->name();
  size_t n = connections_.erase(conn->name());
  assert(n==1);(void) n;
  loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
```



#### 2. TcpConnection使用Buffer作为输入缓冲

使用Buffer读取数据

~~~C++
void TcpConnection::handleRead(Timestamp receiveTime)
{
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readfd(channel_->fd(), &savedErrno);
  if (n > 0) {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = savedErrno;
    LOG << "TcpConnection::handleRead";
    handleError();
  }
}
~~~



#### 3. TcpConnection 发送数据

发送数据比接收数据更难，因为发送数据是主动的，接收读取数据是被动的。

调用WriteCallback,muduo采用水平触发，因此我们只在需要时才关注writable事件

Channel 中添加判断是否正在写的事件

~~~C++
  bool isWriting() const {return events_ & kWriteEvent;}
~~~

TcpConnection的接口中增加了send()和shutdown()两个函数，这两个函数都可以跨线程调用

~~~C++
  //thread safe
  void send(const std::string& message);

  //thread safe
  void shutdown();
~~~

同时TcpConnection的状态增加到了4个

~~~c++
 enum StateE{kConnecting,kConnected,kDisConnected,kDisconnecting};
~~~



其内部实现增加了两个*InLoop成员函数，对应前面的两个新接口函数，并使用Buffer作为输出缓冲区

~~~C++
void sendInLoop(const std::string& message);
void shutdownInLoop();
~~~



~~~C++
 Buffer inputBuffer_;
 Buffer outputBuffer_;
~~~

TcpConnection 状态图

![TcpConnection_status](/img/img_20.png)

shutdown()只是关闭写事件，而handleClose()确实是关闭了整个连接

shutdown()是线程安全的，它会把实际工作放到shutdownInloop()中来做，后者保证在IO线程调用。如果当前没有正在写入，则关闭写入端

~~~C++
void TcpConnection::shutdown(){
  if(state_ == kConnected) {
    setState(kDisconnecting);

    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop() {
  loop_->assertInLoopThread();
  if(!channel_->isWriting()) {
    socket_->shutdownWrite();
  }
}

void Socket::shutdownWrite()
{
  sockets::shutdownWrite(sockfd_);
}

void sockets::shutdownWrite(int sockfd)
{
  if (::shutdown(sockfd, SHUT_WR) < 0)
  {
    LOG << "sockets::shutdownWrite";
  }
}
~~~



send()也是一样的，如果在非IO线程调用，它会把message复制一份，传给IO线程中的sendInLoop()来发送

sendInLoop()会先尝试直接发送数据，如果一次发送完毕就不会启用WriteCallback；如果只发送了部分数据，则把剩余的数据放入outputBuffer_,并开始关注writable事件，以后在handleWrite()中发送剩余数据，如果当前outputBuffer_已经有待发送的数据，那么就不能先尝试发送了，因为这会造成数据乱序

当socket变得可写时，Channel会调用TcpConnection::handleWrite(),这里我们继续发送outputBuffer_中的数据。一旦发送完毕，立即停止观察writable事件，另外如果此时连接正在关闭，则调用shutdownInLoop()，继续执行关闭过程.

~~~C++
void TcpConnection::send(const std::string& message) {
  if(state_ == kConnected) {
    if(loop_->isInLoopThread()) {
      sendInLoop(message);
    }else {
      loop_->runInLoop([this, message] { sendInLoop(message); });
    }
  }
}

void TcpConnection::sendInLoop(const std::string& message) {
  loop_->assertInLoopThread();
  ssize_t  nwrote = 0;

  if(!channel_->isWriting()&& outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(),message.data(),message.size());
    if(nwrote>=0) {
      if(static_cast<size_t>(nwrote) < message.size()) {
        LOG << "I am going to write more data";
      }
    }else {
      nwrote = 0;
      if(errno!=EWOULDBLOCK) {
        LOG<<"TcpConnection::sendInLoop";
      }
    }
  }
  
  
void TcpConnection::handleWrite(){
  loop_->assertInLoopThread();
  if(channel_->isWriting()) {
    ssize_t n = ::write(channel_->fd(),outputBuffer_.peek(),outputBuffer_.readableBytes());
    if(n>0) {
      //将这些字节从outputBuffer_中移除
      outputBuffer_.retrieve(n);
      if(outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting(); //停止监听写事件
        if(state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }else {
        LOG<<"I am going to write more data";
      }
    }else {
      LOG<<"TcpConnection::handleWrite";
    }
  }else {
    LOG<<"Connection is down ,no more writing";
  }
}

~~~

