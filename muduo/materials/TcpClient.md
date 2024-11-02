#  TcpClient

TcpClient与TcpServer有几分相似，都有newConnection和removeConnection这两个成员函数，只不过每个TcpClient只负责管理一个TcpConnection

注意：

在 `TcpClient.cpp` 中，`disconnect()` 和 `stop()` 的区别如下：

1. **`disconnect()` 使用 `connection_` 来断开连接**：  
   `disconnect()` 方法用于在**连接已建立**的情况下主动断开当前连接。此方法会检查并使用 `connection_`（`TcpConnection` 对象）执行断开流程，例如通过 `shutdown()` 关闭连接。在断开连接过程中，它会安全地关闭现有的 TCP 连接并清理资源。换句话说，`disconnect()` 仅在连接已经建立且 `connection_` 有效的情况下工作。
2. **`stop()` 使用 `connector_` 停止尝试连接**：  
   `stop()` 方法用于停止**尚未建立的连接尝试**。`TcpClient` 可以处于一种还在尝试连接的状态，而未真正建立连接，此时 `connection_` 为空。在这种情况下，`stop()` 调用 `connector_` 的 `stop()` 方法，取消连接尝试，并防止 `TcpClient` 进一步尝试与服务器建立连接。
3.

- `disconnect()` 针对已建立的连接，通过 `connection_` 断开连接。
- `stop()` 针对未成功建立的连接尝试，通过 `connector_` 停止连接请求。



~~~C++

using namespace muduo;


namespace muduo {

namespace detail {
void removeConnection(EventLoop* loop,const TcpConnectionPtr& conn) {
  loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
}

void removeConnector(const ConnectorPtr& connector) {

}

}
}  // namespace muduo

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr,const MutexLockGuard& mutex)
  : loop_(loop),                       // 初始化事件循环
    connector_(std::make_shared<Connector>(loop, serverAddr)), // 使用 std::make_shared 创建连接器
    retry_(false),                                    // 初始化重试标志
    connect_(true),                                   // 初始化连接标志
    nextConnId_(1),                            // 初始化下一个连接 ID
    mutex_(mutex)
{

}

TcpClient::~TcpClient()
{
  LOG<< "TcpClient::~TcpClient[" << this
           << "] - connector " << connector_.get();  // 记录析构日志
  TcpConnectionPtr conn;
  {
    MutexLockGuard lock(mutex_);
    conn = connection_;
  }
  if (conn)
  {
    CloseCallback cb = std::bind(&detail::removeConnection, loop_, std::placeholders::_1);
    loop_->runInLoop(
        std::bind(&TcpConnection::setCloseCallback, conn, cb));
  }
  else
  {
    connector_->stop();
    loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
  }
}

void TcpClient::connect() {
  LOG<<"TcpClient::connect["<<this<<"] - connecting to"<< connector_->serverAddress().toHostPort();
  connect_ = true;
  connector_->start();
}

//在连接已建立的情况下主动断开当前连接 connection
void TcpClient::disconnect(){
  connect_ = false;
  {
    MutexLockGuard lock(mutex_);
    if(connection_) {
      connection_->shutdown();
    }
  }
}

//connector_ 是连接器对象（通常是 Connector 类的实例），负责发起连接
//connection_：connection_ 是实际的连接对象（通常是 TcpConnection 类的实例），负责管理已建立的连接



//停止尚未建立的连接尝试 connector
void TcpClient::stop() {
  connect_ = false;
  connector_ ->stop();
}


void TcpClient::newConnection(int sockfd)
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toHostPort().c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = buf;

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  TcpConnectionPtr conn(new TcpConnection(loop_,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpClient::removeConnection, this, std::placeholders::_1));
  {
    MutexLockGuard lock(mutex_);
    connection_ = conn;
  }
  conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  assert(loop_ == conn->getLoop());

  {
    MutexLockGuard lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();
  }

  loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  if (retry_ && connect_)
  {
    LOG << "TcpClient::connect[" << this << "] - Reconnecting to "
             << connector_->serverAddress().toHostPort();
    connector_->restart();
  }
}

//
// Created by 刘畅 on 24-10-31.
//

#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <memory>
#include "../muduo/thread/Mutex.h"
#include "TcpConnection.h"

namespace muduo {

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient :public std:: enable_shared_from_this<TcpClient> {
public:
  TcpClient(EventLoop* loop,const InetAddress& serverAddr,const MutexLockGuard& mutex) ; //初始化事件循环和服务器地址
  ~TcpClient();

  void connect();  //建立连接
  void disconnect(); //断开连接
  void stop();  //停止连接

  TcpConnectionPtr connection() const {  //获取当前连接
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  bool retry() const; //返回是否重试标志
  void enableRetry(){retry_ = true;}  //允许重试连接

  void setConnectionCallback(const ConnectionCallback& cb){connectionCallback_ = cb;}

  void setMessageCallback(const MessageCallback& cb){messageCallback_ = cb;}

  void setWriteCompleteCallback(const WriteCompleteCallback& cb){writeCompleteCallback_ = cb;}


private:
  void newConnection(int sockfd); //新建连接
  void removeConnection(const TcpConnectionPtr& conn); //移除连接

  EventLoop* loop_;  //事件循环指针
  ConnectorPtr connector_; //连接器指针
  ConnectionCallback connectionCallback_;    // 连接回调
  MessageCallback messageCallback_; //消息回调
  WriteCompleteCallback writeCompleteCallback_; //写完成回调
  bool retry_; //重试标志
  bool connect_; //是否连接标志
  int nextConnId_; //下一个连接ID
  MutexLockGuard mutex_;
  TcpConnectionPtr connection_; //TCP连接指针
};

}

#endif //TCPCLIENT_H
~~~

`TcpClient.cpp` 文件实现了 `TcpClient` 类，这是 Muduo 网络库中的一个 TCP 客户端类，用于管理客户端与服务器之间的 TCP 连接。这个类包含连接的建立、断开、重试机制等，便于在应用中更灵活地控制连接。下面是对 `TcpClient` 类的简要分析：

##### 类的作用

`TcpClient` 主要用于建立和管理与服务器的 TCP 连接。它可以控制连接的状态、处理连接失败后的重试，并提供设置连接、消息和写入完成等回调的接口。其主要成员包括 `loop_`（事件循环）、`connector_`（连接管理器）、`connection_`（连接对象）、以及几个回调函数。

##### 构造函数和析构函数

- **`TcpClient(EventLoop* loop, const InetAddress& serverAddr)`**：  
  初始化 `TcpClient` 对象，指定 `loop_` 为事件循环指针，`serverAddr` 为服务器地址。创建 `connector_` 并注册新连接的回调函数 `newConnection()`。

- **`~TcpClient()`**：  
  析构函数，安全断开连接并清理资源。如果有现有连接，将设置关闭回调并在 `loop_` 中执行；否则停止连接尝试。

##### 成员函数

- **`connect()`**：  
  开始尝试与服务器建立连接。将 `connect_` 置为 `true` 并启动 `connector_`。

- **`disconnect()`**：  
  主动断开当前已建立的连接。如果连接存在，调用 `shutdown()` 方法来关闭连接，同时将 `connect_` 置为 `false`。

- **`stop()`**：  
  停止连接尝试。适用于在连接尚未成功建立时，通过 `connector_` 停止连接过程，将 `connect_` 置为 `false`。

- **`connection()`**：  
  返回当前的连接指针 `connection_`，加锁以保证线程安全。

- **`retry()` 和 `enableRetry()`**：  
  获取和设置重试标志 `retry_`。当连接断开时，如果设置了重试，`TcpClient` 会尝试重新连接。

- **`setConnectionCallback()`**、**`setMessageCallback()`**、**`setWriteCompleteCallback()`**：  
  分别用于设置连接回调、消息回调和写入完成回调。这些回调函数在连接建立、收到消息、完成数据写入时被调用，允许用户自定义处理逻辑。

##### 私有函数

- **`newConnection(int sockfd)`**：  
  新连接的回调函数，当连接成功时由 `connector_` 调用。该函数创建一个 `TcpConnection` 对象并注册相应的回调，同时将连接指针保存到 `connection_`。

- **`removeConnection(const TcpConnectionPtr& conn)`**：  
  连接移除的回调函数，在连接断开时调用。它将 `connection_` 指针置空并执行 `connectDestroyed()` 以清理资源，若 `retry_` 和 `connect_` 标志均为 `true`，则重新启动连接。

##### 整体作用

`TcpClient` 类在客户端应用程序中提供了一个可靠的 TCP 连接管理方式。通过封装连接的建立、断开、重试等操作，`TcpClient` 简化了应用对 TCP 连接的控制。同时，提供了多个回调函数接口，让用户可以轻松自定义在连接、消息接收、写入完成等事件下的行为。这种设计使 `TcpClient` 适合需要长时间保持连接的网络应用，例如 IM、在线游戏或实时数据同步系统。

