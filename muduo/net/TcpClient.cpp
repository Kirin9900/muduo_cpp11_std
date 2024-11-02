

#include "TcpClient.h"
#include "Connector.h"
#include "EventLoop.h"
#include "SocketsOps.h"

#include "log/base/Logging.h"
#include <functional>

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

TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),                       // 初始化事件循环
    connector_(std::make_shared<Connector>(loop, serverAddr)), // 使用 std::make_shared 创建连接器
    retry_(false),                                    // 初始化重试标志
    connect_(true),                                   // 初始化连接标志
    nextConnId_(1)
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







