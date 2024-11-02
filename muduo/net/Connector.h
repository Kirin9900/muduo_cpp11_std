#ifndef CONNECTOR_H
#define CONNECTOR_H

#include "../net/InetAddress.h"
#include "TimerId.h"
#include <functional>
#include <memory>

namespace muduo {

class Channel;
class EventLoop;

class Connector {
public:
  using NewConnectionCallback = std::function<void(int sockfd)>;

  Connector(EventLoop* loop,const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb) {

  }

  void start();
  void restart();
  void stop();

  const InetAddress& serverAddress() const { return serverAddr_; }

private:
  enum States { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30 * 1000;
  static const int kInitRetryDelayMs = 500;

  void setState(States s) { state_ = s; }
  void startInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

  EventLoop* loop_;
  InetAddress serverAddr_;
  bool connect_;  // 原子操作标志
  States state_;  // 状态标志
  std::unique_ptr<Channel> channel_;  // 使用unique_ptr替代boost::scoped_ptr
  NewConnectionCallback newConnectionCallback_;
  int retryDelayMs_;
  TimerId timerId_{};
};

}

#endif //CONNECTOR_H
