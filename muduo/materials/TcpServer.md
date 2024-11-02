# TcpServer

TcpServer新建连接的相关函数的调用顺序如下图所示

![TcpServer](/img/img_18.png)

其中Channel::handleEvent()的触发条件是listening socket可读，表明有新连接到达。TcpServer会为新连接创建相应的TcpConnection对象





TcpServer class 的功能是管理accept获得的TcpConnection.TcpServer是供用户直接使用的，生命期由用户控制

用户只需要设置好callback，再调用start()即可



~~~C++
class TcpServer {
public:
  TcpServer(EventLoop* loop,const InetAddress& listenAddr);
  ~TcpServer();

  void start();

  void setConnectionCallback(const ConnectionCallback& cb) {connectionCallback_ = cb;}

  void setMessageCallback(const MessageCallback& cb) {messageCallback_ = cb;}
~~~



TcpServer内部使用Acceptor 来获得新连接的fd，它保存用户提供的ConnectionCallback和MessageCallback，在新建TcpConnection的时候会原样传给后者，TcpServer持有目前存活的TcpConnection的shared_ptr,用户也可以持有TcpConnectionPtr

~~~C++
class TcpServer {
public:
  TcpServer(EventLoop* loop,const InetAddress& listenAddr);
  ~TcpServer();

  void start();

  void setConnectionCallback(const ConnectionCallback& cb) {connectionCallback_ = cb;}

  void setMessageCallback(const MessageCallback& cb) {messageCallback_ = cb;}
~~~

每个TcpConnection对象有一个名字，这个名字是由所属的TcpServer在创建TcpConnection对象时生成，名字是ConnectionMap的key

在新连接到达时，Acceptor会回调newConnection(),后者会创建TcpConnection对象conn，把它加入ConnectMap,设置好callback,再调用conn->connectEstablished(),其中会回调用户提供的ConnectionCallback.

~~~C++
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
  loop_->assertInLoopThread();

  char buf[32];
  snprintf(buf,sizeof buf,"#%d",nextConnId_);
  ++nextConnId_;

  std::string connName = name_+buf;
  LOG<<"TcpServer::new Connection ["<<name_
  <<"] -新的连接 ["<<connName<<"] 来自"<<peerAddr.toHostPort();

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  TcpConnectionPtr conn(std::make_shared<TcpConnection>(loop_,connName,sockfd,localAddr,peerAddr));
  connections_[connName] = conn;

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);

  conn->connectEstablished();

}
~~~


## 关键代码

`TcpServer` 类是 Muduo 网络库中用于管理 TCP 服务端的类，它负责监听端口、接受新连接，并将连接交给 `TcpConnection` 类进行数据传输管理。以下是对该类的成员变量和函数的详细解析：

### 成员变量解析

```cpp
EventLoop* loop_;
const std::string name_;
std::unique_ptr<Acceptor> acceptor_;
std::shared_ptr<EventLoopThreadPool> threadPool_;
std::atomic<bool> started_;
int nextConnId_;
std::unordered_map<std::string, TcpConnectionPtr> connections_;
```

1. **`EventLoop* loop_;`**
    - 指向事件循环对象的指针，用于管理当前服务器的 I/O 操作。`loop_` 管理所有的连接请求、回调等事件。

2. **`const std::string name_;`**
    - 服务器的名称，通常由监听地址的端口号构成，用于唯一标识该服务器。

3. **`std::unique_ptr<Acceptor> acceptor_;`**
    - 接收连接的对象指针，用于在指定端口监听并接收新连接请求。

4. **`std::shared_ptr<EventLoopThreadPool> threadPool_;`**
    - 线程池对象指针，用于分配事件循环线程，将连接分配给不同的 I/O 线程以提高并发处理能力。

5. **`std::atomic<bool> started_;`**
    - 原子布尔值，表示服务器是否已启动，防止多次启动服务器。

6. **`int nextConnId_;`**
    - 用于生成每个新连接的唯一 ID，每次新连接建立时递增。

7. **`std::unordered_map<std::string, TcpConnectionPtr> connections_;`**
    - 存储当前所有活动连接的容器，键为连接名称，值为 `TcpConnection` 智能指针，便于管理连接的生命周期。

### 成员函数解析

#### `TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr)`

构造函数，用于初始化服务器对象：

- **`loop_(loop)`**：将 `loop_` 设置为主事件循环。
- **`name_(listenAddr.toHostPort())`**：服务器名称由监听地址转换为主机端口的字符串表示。
- **`acceptor_(new Acceptor(loop, listenAddr))`**：创建 `Acceptor`，并绑定 `loop_`，同时在指定的 `listenAddr` 地址上监听。
- **`acceptor_->setNewConnectionCallback(...)`**：设置新连接回调函数，当有新连接时会触发 `newConnection` 方法。

#### `void TcpServer::setThreadNum(int numThreads)`

设置线程池的线程数量：

- **`threadPool_->setThreadNum(numThreads);`**：将线程池线程数量设置为指定的 `numThreads`，并发提高服务器性能。

#### `void TcpServer::start()`

启动服务器：

- **`if (!started_)`**：检查服务器是否已经启动，避免重复启动。
- **`started_ = true;`**：标记为启动状态。
- **`loop_->runInLoop(...)`**：调用 `Acceptor` 的 `listen` 方法，使其在 `loop_` 中启动监听。

#### `void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)`

当有新连接到来时执行的回调函数：

1. **`loop_->assertInLoopThread();`**：确保在 I/O 线程中调用该方法。
2. **`snprintf(...)`**：生成连接名称，并通过 `nextConnId_` 唯一标识每个新连接。
3. **`LOG<<"TcpServer::new Connection ["<<name_...`**：记录日志，输出新连接的名称和客户端地址。
4. **`InetAddress localAddr(sockets::getLocalAddr(sockfd));`**：获取本地地址并封装为 `InetAddress`。
5. **`EventLoop* ioLoop = threadPool_->getNextLoop();`**：从线程池中获取下一个事件循环，分配新连接的 I/O 操作。
6. **`TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr));`**：创建 `TcpConnection` 对象，管理连接。
7. **`connections_[connName] = conn;`**：将新连接插入到连接映射中，便于管理。
8. **`conn->setConnectionCallback(...)`**：为连接设置各类回调函数，包括连接、消息、写入完成、关闭等事件。
9. **`ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));`**：在分配的 I/O 线程中执行连接建立通知，使得新连接进入工作状态。

#### `void TcpServer::removeConnection(const TcpConnectionPtr& conn)`

当连接关闭时调用，移除连接并清理资源：

1. **`loop_->runInLoop(...)`**：确保在 `loop_` 中调用 `removeConnectionInLoop` 进行连接移除。

#### `void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)`

真正执行连接移除操作：

1. **`loop_->assertInLoopThread();`**：确保在 I/O 线程中执行。
2. **`connections_.erase(conn->name());`**：从连接映射中移除该连接。
3. **`ioLoop->runInLoop(...)`**：通知 `TcpConnection` 对象该连接已销毁，执行清理工作。

### 整体作用

`TcpServer` 管理 TCP 服务端的整个生命周期，包括监听端口、接受连接、创建连接对象以及处理连接关闭。通过使用线程池、事件循环和回调函数，使得该类具有良好的并发性能和灵活的事件处理能力。
