# 多线程TcpServer



## 1. EventLoopThreadPool

用one loop per thread的思想实现多线程TcpServer的关键步骤是在新建TCpConnection时从eventloop poll里挑选一个loop给TcpConnection用。也就是说多线程TcpServer自己的EvenLoop只用来接受新连接，而新连接还会用其他EventLoop来执行IO(单线程TcpServer的EventLoop是与TcpConnection共享的)



~~~C++
namespace muduo {
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
public:
  EventLoopThreadPool(EventLoop * baseLoop);
  ~EventLoopThreadPool();
  void start();
  void setThreadNum(int numThreads){}
  EventLoop* getNextLoop();

private:
  EventLoop* baseLoop_;
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;
};
~~~

TcpServer 每次新建一个TcpConnection就会调用getNextLoop()来取得EventLoop.如果是单线程服务，每次返回的是baseLoop_,即TcpServer自己用的那个loop.其中serThreadNum()的参数的意义设定为线程的个数

TcpServer只需要增加代表线程池的智能指针

同时也需要将通知建立连接和销毁连接都需要进入loop进行事件循环





#### 2. Connector

主动发起连接比被动接受连接要复杂一些，一方面是错误处理麻烦，另一方面时要考虑重试。在非阻塞网络编程中，发起连接的基本方式是调用connect，当socket变得可写时表明连接建立完成。我们封装为Connector class

Connector只负责建立socket连接，不负责创建TcpConnection，它的NewConnectionCallback回调的参数是socket文件描述符

~~~C++

using namespace muduo;

const int Connector::kMaxRetryDelayMs; //初始化最大重试延时

Connector::Connector(EventLoop* loop,const InetAddress&serverAddr)
  : loop_(loop),  //事件循环
    serverAddr_(serverAddr), //服务器地址
    connect_(false),  //初始化连接标志为false
    state_(kDisconnected), //初始状态为未连接
    retryDelayMs_(kInitRetryDelayMs), //初始重试延时
    timerId_(nullptr)
{
  LOG<<"ctor["<<this<<"]";
}

Connector::~Connector() {
  LOG << "dtor[" << this << "]";
 //loop_->cancel(timerId_)
  assert(!channel_);
}

void Connector::start() {
  connect_ = true; //设置连接标识为true
  loop_->runInLoop([this]{startInLoop();}); //将startInLoop函数投递到事件循环中执行
}

void Connector::startInLoop(){
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected); //确保当前状态为未连接
  if(connect_) {   //如果需要连接
    connect();
  }else {
    LOG<<"do not connect";
  }
}

void Connector::connect()
{
  int sockfd = sockets::createNonblockingOrDie();   // 创建非阻塞套接字
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());   // 尝试连接
  int savedErrno = (ret == 0) ? 0 : errno;   // 获取错误码

  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);   // 如果没有错误或可以继续连接，调用connecting方法
    break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);   // 如果是可重试的错误，调用retry方法
      break;

    default:
      LOG << "Unexpected error in Connector::startInLoop " << savedErrno;   // 记录不可预期的错误
    sockets::close(sockfd);   // 关闭套接字
    break;
  }
}

void Connector::restart(){
  loop_->assertInLoopThread();
  setState(kDisconnected);  //重置为未连接状态
  retryDelayMs_ = kInitRetryDelayMs;  //重置重试延时
  connect_ = true;  //设置连接标志
  startInLoop();  //重新启动连接
}

void Connector::stop() {
  connect_ = false;  //取消连接标志
  //loop_->cancel(timerId_);
}

void Connector::connecting(int sockfd) {
  setState(kConnecting); //更新状态为连接中
  assert(!channel_);  //确保没有已存在的通道
  channel_ = std::make_unique<Channel>(loop_,sockfd); //创建新的通道

  channel_ ->setWriteCallback([this]{handleWrite();}); //设置写事件回调
  channel_->setErrorCallback([this]{handleError();});  //设置错误事件回调

  channel_ ->enableWriting();  //启用写事件
}

int Connector::removeAndResetChannel(){
  channel_->disableAll();  //禁用通道所有事件
  loop_->removeChannel(channel_.get()); //从事件循环中移除通道
  int sockfd = channel_->fd(); //获取套接字描述符
  loop_->queueInLoop([this]{resetChannel();}); //将resetChannel投递到事件循环中执行
  return sockfd; //返回套接字描述符
}

void Connector::resetChannel(){
  channel_.reset();
}

void Connector::handleWrite(){
  LOG<<"Connector::handleWrite"<<state_;
  if(state_ == kConnecting) {
    int sockfd = removeAndResetChannel(); //删除并重置通道
    int err = sockets::getSocketError(sockfd); //获取套接字错误信息
    if(err) {
      LOG<<"Connector::handleWrite -SO_ERROR ="<<err<<" "<<strerror(err);
      retry(sockfd); //调用retry方法重新连接
    }else {
      setState(kConnected); //更新状态为已连接
      if(connect_) {
        newConnectionCallback_(sockfd); //调用新连接回调
      }else {
        sockets::close(sockfd); //关闭套接字
      }
    }
  }else {
    assert(state_ == kDisconnected);
  }
}


void Connector::handleError() {
  LOG << "Connector::handleError";   // 记录错误日志
  assert(state_ == kConnecting);   // 确保当前状态为连接中

  int sockfd = removeAndResetChannel();   // 删除并重置通道
  int err = sockets::getSocketError(sockfd);
  LOG << "SO_ERROR = " << err << " " << strerror(err);
  retry(sockfd);   // 调用retry方法重新连接
}

void Connector::retry(int sockfd)
{
  sockets::close(sockfd);   // 关闭套接字
  setState(kDisconnected);  // 设置状态为未连接
  if (connect_)   // 如果需要连接
  {
    LOG << "Connector::retry - Retry connecting to "<< serverAddr_.toHostPort()
    << "in " << retryDelayMs_ << " milliseconds.";   // 记录信息日志
    timerId_ = loop_->runAfter(retryDelayMs_ / 1000.0, [this] { startInLoop(); });   // 设置定时器重新启动连接
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);   // 延长重试延时
  }
  else
  {
    LOG << "do not connect";   // 记录调试日志
  }
}
~~~



##### 整体概述

`Connector` 类是一个管理客户端连接的组件，主要用于在事件驱动的网络库中发起和管理连接。它支持异步连接、自动重试和错误处理，适合在高并发网络应用中使用。此类依赖于 `EventLoop`、`Channel` 等辅助类，通过事件循环机制来触发连接、写事件、错误事件等操作。

##### 成员变量概述

- **loop_**：指向 `EventLoop`，用于管理事件循环。
- **serverAddr_**：服务器地址信息（`InetAddress`），包含连接目标的地址。
- **connect_**：标记当前连接状态，表示是否需要连接。
- **state_**：表示当前连接状态，有 `kDisconnected`、`kConnecting`、`kConnected` 三种。
- **channel_**：智能指针，指向 `Channel` 对象，用于监听和处理套接字上的事件。
- **newConnectionCallback_**：新的连接回调函数，在成功建立连接后触发。
- **retryDelayMs_**：重试延迟，初始化为500ms，在每次重试后倍增，直到达到最大值 `kMaxRetryDelayMs`。
- **timerId_**：定时器ID，用于管理重试时的定时器。

##### 成员函数概述

- **Connector(EventLoop* loop, const InetAddress& serverAddr)**：构造函数，初始化连接信息并设定初始重试延迟。
- **~Connector()**：析构函数，取消定时器，确保通道已重置。
- **setNewConnectionCallback(const NewConnectionCallback& cb)**：设置新连接成功时的回调函数。
- **start()**：开启连接，可在任何线程调用，将连接任务投递到事件循环中执行。
- **startInLoop()**：在事件循环中实际启动连接。确保只有在未连接状态下才会发起连接。
- **connect()**：创建非阻塞套接字并尝试连接。根据连接结果，决定调用 `connecting()` 或 `retry()`。
- **restart()**：重新启动连接，将状态重置为未连接，并将重试延时恢复为初始值。
- **stop()**：停止连接，取消定时器，并设置连接标志为 `false`。
- **connecting(int sockfd)**：设置状态为连接中，初始化 `Channel` 对象，并注册写事件和错误事件的回调。
- **removeAndResetChannel()**：禁用通道事件，从事件循环中移除并重置通道。
- **resetChannel()**：重置 `Channel` 对象。
- **handleWrite()**：处理写事件，成功时调用新连接回调函数，失败时调用 `retry()` 重试。
- **handleError()**：处理错误事件，记录错误信息并调用 `retry()` 重试。
- **retry(int sockfd)**：重试连接。关闭当前套接字，设置状态为未连接，并根据当前重试延迟设置定时器以再次启动连接。