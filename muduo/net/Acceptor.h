

#ifndef ACCEPTOR_H
#define ACCEPTOR_H
#include <functional>

#include "Channel.h"
#include "Socket.h"

using namespace muduo;

namespace muduo {
class EventLoop;
class InetAddress;
class Acceptor {
public:
  typedef std::function<void (int sockfd,
                              const InetAddress&)> NewConnectionCallback;
  Acceptor(EventLoop* loop,const InetAddress& listenAddr);
  void setNewConnectionCallback(const NewConnectionCallback& cb) {

  }
  bool listening()const{return listening_;}
  void listen();

private:
  void handleRead();

  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
};


}


#endif //ACCEPTOR_H
