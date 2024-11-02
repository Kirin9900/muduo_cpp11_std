

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <map>

#include "../EventLoopThreadPool.h"
#include "../Callbacks.h"
#include "../EventLoop.h"
#include "InetAddress.h"

namespace muduo {
class Acceptor;
class Eventloop;

class TcpServer {
public:
  TcpServer(EventLoop* loop,const InetAddress& listenAddr);
  ~TcpServer();

  void setThreadNum(int numThreads);

  void start();

  void setConnectionCallback(const ConnectionCallback& cb) {connectionCallback_ = cb;}

  void setMessageCallback(const MessageCallback& cb) {messageCallback_ = cb;}

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

private:

  void newConnection(int sockfd,const InetAddress& peerAddr);
  void removeConnection(const TcpConnectionPtr& conn);

  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  typedef std::map<std::string,TcpConnectionPtr> ConnectionMap;

  EventLoop* loop_;
  const std::string name_;
  std::shared_ptr<Acceptor> acceptor_{};
  ConnectionCallback connectionCallback_{};
  MessageCallback messageCallback_{};
  WriteCompleteCallback writeCompleteCallback_;
  bool started_;
  int nextConnId_;
  ConnectionMap connections_{};
  std::unique_ptr<EventLoopThreadPool> threadPool_;


};
}




#endif //TCPSERVER_H
