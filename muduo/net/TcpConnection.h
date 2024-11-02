

#ifndef TCPCONNECTION_H
#define TCPCONNECTION_H

#include <functional>
#include <memory>
#include <string>


#include "../Callbacks.h"
#include "Buffer.h"
#include "InetAddress.h"
#include "TimeStamp.h"

namespace muduo
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP 连接类，适用于客户端和服务器端
///
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
 public:
  /// 构造函数，使用已连接的 sockfd 创建 TcpConnection
  ///
  /// 用户不应该直接创建该对象
  TcpConnection(EventLoop* loop,
                const std::string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }   // 获取事件循环对象指针
  const std::string& name() const { return name_; }  // 获取连接名称
  const InetAddress& localAddress() { return localAddr_; }  // 获取本地地址
  const InetAddress& peerAddress() { return peerAddr_; }    // 获取远程地址
  bool connected() const { return state_ == kConnected; }   // 判断是否已连接

  // 线程安全地发送数据
  void send(const std::string& message);
  // 线程安全地关闭连接
  void shutdown();
  void setTcpNoDelay(bool on);  // 设置 TCP_NO_DELAY 选项

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; } // 设置连接回调

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }   // 设置消息回调

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }  // 设置写完成回调

  /// 仅供内部使用
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }  // 设置关闭回调

  // TcpServer 接受新连接时调用
  void connectEstablished();   // 仅应调用一次
  // TcpServer 从映射中移除连接时调用
  void connectDestroyed();  // 仅应调用一次

 private:
  enum StateE { kConnecting, kConnected, kDisconnecting, kDisconnected, }; // 定义状态

  void setState(StateE s) { state_ = s; } // 设置状态
  void handleRead(Timestamp receiveTime);  // 处理读事件
  void handleWrite();  // 处理写事件
  void handleClose();  // 处理关闭事件
  void handleError();  // 处理错误事件
  void sendInLoop(const std::string& message);  // 在循环中发送
  void shutdownInLoop();  // 在循环中关闭连接

  EventLoop* loop_;        // 事件循环
  std::string name_;       // 连接名称
  StateE state_;           // 连接状态，使用原子变量改进
  std::unique_ptr<Socket> socket_;   // Socket 对象
  std::unique_ptr<Channel> channel_; // Channel 对象
  InetAddress localAddr_;   // 本地地址
  InetAddress peerAddr_;    // 远程地址
  ConnectionCallback connectionCallback_;  // 连接回调
  MessageCallback messageCallback_;        // 消息回调
  WriteCompleteCallback writeCompleteCallback_;  // 写完成回调
  CloseCallback closeCallback_;            // 关闭回调
  Buffer inputBuffer_;    // 输入缓冲区
  Buffer outputBuffer_;   // 输出缓冲区
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr; // 使用标准库智能指针

}  // namespace muduo


#endif //TCPCONNECTION_H
