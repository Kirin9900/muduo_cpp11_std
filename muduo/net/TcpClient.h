

#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <memory>
#include "thread/Mutex.h"
#include "TcpConnection.h"

namespace muduo {

class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

class TcpClient :public std:: enable_shared_from_this<TcpClient> {
public:
  TcpClient(EventLoop* loop,const InetAddress& serverAddr) ; //初始化事件循环和服务器地址
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
  mutable MutexLock mutex_;
  TcpConnectionPtr connection_; //TCP连接指针
};

}





#endif //TCPCLIENT_H
