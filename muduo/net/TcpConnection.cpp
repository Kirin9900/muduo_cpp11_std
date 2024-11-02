#include "TcpConnection.h"

#include "../log/base/Logging.h"
#include "../Channel.h"
#include "../EventLoop.h"
#include "Socket.h"
#include "../SocketsOps.h"

#include <functional>  // 替换 boost::bind
#include <cerrno>
#include <cstdio>

using namespace muduo;

TcpConnection::TcpConnection(EventLoop* loop,
                             const std::string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(loop),  // 初始化事件循环
    name_(nameArg),              // 设置连接名称
    state_(kConnecting),         // 初始状态为连接中
    socket_(new Socket(sockfd)), // 创建 Socket 对象
    channel_(new Channel(loop, sockfd)), // 创建 Channel 对象
    localAddr_(localAddr),       // 本地地址
    peerAddr_(peerAddr)          // 远程地址
{
  LOG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  channel_->setWReadCallback(
      std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)); // 设置读回调函数
  channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this)); // 设置写回调函数
  channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this)); // 设置关闭回调函数
  channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this)); // 设置错误回调函数
}

TcpConnection::~TcpConnection()
{
  LOG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd();
}

void TcpConnection::send(const std::string& message)
{
  if (state_ == kConnected) {  // 如果连接已建立
    if (loop_->isInLoopThread()) {
      sendInLoop(message);  // 直接在循环线程中发送
    } else {
      loop_->runInLoop(
          std::bind(&TcpConnection::sendInLoop, this, message)); // 调用 sendInLoop
    }
  }
}

void TcpConnection::sendInLoop(const std::string& message)
{
  loop_->assertInLoopThread();  // 确保在循环线程中调用
  ssize_t nwrote = 0;
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) { // 如果没有在写
    nwrote = ::write(channel_->fd(), message.data(), message.size()); // 直接写入
    if (nwrote >= 0) {
      if (static_cast<size_t>(nwrote) < message.size()) {
        LOG << "I am going to write more data";
      } else if (writeCompleteCallback_) {
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this())); // 写完成回调
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG << "TcpConnection::sendInLoop";
      }
    }
  }

  assert(nwrote >= 0);
  if (static_cast<size_t>(nwrote) < message.size()) {  // 如果没写完，加入输出缓冲区
    outputBuffer_.append(message.data() + nwrote, message.size() - nwrote);
    if (!channel_->isWriting()) {
      channel_->enableWriting();  // 启用写事件
    }
  }
}

void TcpConnection::shutdown()
{
  if (state_ == kConnected) { // 如果已连接
    setState(kDisconnecting); // 设置状态为正在断开连接
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this)); // 异步关闭连接
  }
}

void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();  // 确保在循环线程中调用
  if (!channel_->isWriting()) { // 如果没有写入，直接关闭写
    socket_->shutdownWrite();
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);  // 设置 TCP_NODELAY 选项
}

void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();  // 确保在循环线程中调用
  assert(state_ == kConnecting);
  setState(kConnected);         // 设置状态为已连接
  channel_->enableReading();    // 启用读事件
  connectionCallback_(shared_from_this()); // 调用连接回调函数
}

void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnected || state_ == kDisconnecting);
  setState(kDisconnected);      // 设置状态为已断开
  channel_->disableAll();       // 禁用所有事件
  connectionCallback_(shared_from_this()); // 调用连接回调

  loop_->removeChannel(channel_.get());    // 移除事件通道
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readfd(channel_->fd(), &savedErrno); // 从 fd 读取数据
  if (n > 0) {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime); // 调用消息回调
  } else if (n == 0) {
    handleClose();  // 关闭连接
  } else {
    errno = savedErrno;
    LOG << "TcpConnection::handleRead";
    handleError();  // 处理错误
  }
}

void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();  // 确保在循环线程中调用
  if (channel_->isWriting()) {  // 如果正在写入
    ssize_t n = ::write(channel_->fd(),
                        outputBuffer_.peek(),
                        outputBuffer_.readableBytes());
    if (n > 0) {
      outputBuffer_.retrieve(n);  // 从缓冲区中取出已写数据
      if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();  // 禁用写事件
        if (writeCompleteCallback_) {
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this())); // 写完成回调
        }
        if (state_ == kDisconnecting) {
          shutdownInLoop();  // 如果正在断开连接，则关闭连接
        }
      } else {
        LOG << "I am going to write more data";
      }
    } else {
      LOG << "TcpConnection::handleWrite";
    }
  } else {
    LOG << "Connection is down, no more writing";
  }
}

void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();  // 确保在循环线程中调用
  LOG << "TcpConnection::handleClose state = " << state_;
  assert(state_ == kConnected || state_ == kDisconnecting);
  channel_->disableAll();       // 禁用所有事件
  closeCallback_(shared_from_this()); // 调用关闭回调
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror(err); // 输出错误日志
}