
#include "EventLoop.h"
#include "EventLoopThread.h"

using namespace muduo;

EventLoopThread::EventLoopThread()
  :loop_(nullptr),
   exiting_(false),
   thread_(std::bind(&EventLoopThread::threadFunc,this)),
   mutex_(),
   cond_(mutex_)
{

}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  loop_->quit();
  thread_.join();
}

EventLoop* EventLoopThread::startLoop() {
  assert(!thread_.started());
  thread_.start();
  {
    MutexLockGuard lock(mutex_);
    while(loop_ == nullptr) {
      cond_.wait();
    }
  }
  return loop_;
}

void EventLoopThread::threadFunc() {
  EventLoop loop;
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }
  loop.loop();
}


