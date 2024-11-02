

#ifndef EVENTLOOPTHREAD_H
#define EVENTLOOPTHREAD_H

#include "../muduo/thread/Mutex.h"
#include "../muduo/thread/Thread.h"
#include "../muduo/thread/Condition.h"




namespace muduo {
class EventLoop;

class EventLoopThread {
public:
  EventLoopThread();
  ~EventLoopThread();
  EventLoop* startLoop();

private:
  void threadFunc();

  EventLoop* loop_;
  bool exiting_;
  Thread thread_;
  MutexLock mutex_;
  Condition cond_;

};
}


#endif //EVENTLOOPTHREAD_H
