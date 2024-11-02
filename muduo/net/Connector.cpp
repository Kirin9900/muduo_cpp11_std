#include "Connector.h"
#include "Channel.h"
#include "EventLoop.h"
#include "SocketsOps.h"
#include "log/base/Logging.h"
#include <cerrno>
#include <cstring>

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
  loop_->cancel(timerId_);
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
  loop_->cancel(timerId_);
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









