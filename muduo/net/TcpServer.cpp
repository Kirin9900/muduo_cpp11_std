
#include "TcpServer.h"

#include <cstdio>
#include <functional>

#include "log/base/Logging.h"
#include "EventLoop.h"
#include "SocketsOps.h"
#include "Acceptor.h"
#include "TcpConnection.h"

using namespace muduo;

//构造函数初始化
TcpServer::TcpServer(EventLoop* loop,const InetAddress& listenAddr)
  : loop_(loop),
    name_(listenAddr.toHostPort()),
    acceptor_(new Acceptor(loop,listenAddr)),
    started_(false),
    nextConnId_(1)
{
  // 设置新的连接回调函数，当有新连接时调用 newConnection 方法
  acceptor_->setNewConnectionCallback(
      [this](auto && PH1, auto && PH2) { newConnection(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); });

}

//析构函数
TcpServer::~TcpServer() = default;


void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

//启动服务器
void TcpServer::start() {

  //如果服务器还未启动，则将其标记为启动
  if(!started_) {
    started_ = true;
  }

  //如果acceptor 尚未开始监听，则在事件循环中调用listen方法
  if(!acceptor_->listening()) {
    loop_->runInLoop([capture0 = acceptor_.get()] { capture0->listen(); });
  }
}

//当有新连接与来时调用的函数
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
  //确保是在IO线程执行
  loop_->assertInLoopThread();

  char buf[32];
  snprintf(buf,sizeof buf,"#%d",nextConnId_);
  ++nextConnId_;

  //生成新连接的名称
  std::string connName = name_+buf;
  LOG<<"TcpServer::new Connection ["<<name_
  <<"] -新的连接 ["<<connName<<"] 来自"<<peerAddr.toHostPort();


  //获取本地地址
  InetAddress localAddr(sockets::getLocalAddr(sockfd));

  EventLoop* ioLoop = threadPool_->getNextLoop();


  //创建新的TcpConnection对象
  TcpConnectionPtr conn(std::make_shared<TcpConnection>(ioLoop,connName, sockfd,
                                                        localAddr, peerAddr));

  // 保存连接map
  connections_[connName] = conn;

  //设置好连接回调函数和消息回调函数
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
    std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

  //通知连接已经建立
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr& conn){
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn) {
  loop_->assertInLoopThread();
  LOG<<"TcpServer::removeConnectionInLoop ["<<name_<<"] - connection"<<conn->name();
  size_t n = connections_.erase(conn->name());
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->runInLoop(std::bind(&TcpConnection::connectDestroyed,conn));;
}



