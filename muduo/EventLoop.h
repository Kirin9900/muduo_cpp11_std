

#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include <sched.h>

#include <memory>

#include "log//base/CurrentThread.h"

#include "Poll.h"
#include "TimerQueue.h"

namespace muduo {
class Poller;
class EventLoop {
public:
  EventLoop();
  ~EventLoop();
  void loop();
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  void assertInLoopThread() {
    if(!isInLoopThread()) {
      abortNotInLoopThread();
    }
  }

  typedef std::function<void()> Functor;

  bool isInLoopThread() const{return threadId_ == CurrentThread::tid();}
  void quit();
  void wakeup();

  void runInLoop(const Functor&cb) ;

  void queueInLoop(const Functor& cb);

  TimerId runAt(const Timestamp& time, const TimerCallback& cb);
  ///
  /// Runs callback after @c delay seconds.
  ///
  TimerId runAfter(double delay, const TimerCallback& cb);
  ///
  /// Runs callback every @c interval seconds.
  ///
  TimerId runEvery(double interval, const TimerCallback& cb);

  void cancel(TimerId timerId);


private:
  void abortNotInLoopThread();
  void handleRead(); //wake up
  void dePendingFunctors();

  typedef std::vector<Channel*> ChannelList;



  bool looping_;  //atomic
  bool quit_;
  bool callingPendingFunctors; //atomic
  const pid_t threadId_;
  Timestamp pollReturnTime_;


  int wakeupFd_;
  std::shared_ptr<Channel> wakeupChannel_;

  std::shared_ptr<Poller> poller_;

  ChannelList activeChannels_;

  std::shared_ptr<TimerQueue> timerQueue_;

  std::vector<Functor> pendingFunctors_;
  MutexLock mutex_;

};


}

#endif //EVENTLOOP_H
