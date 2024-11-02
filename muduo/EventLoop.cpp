

#include "EventLoop.h"

#include <poll.h>
#include <unistd.h>

#include <cassert>

#include "../muduo/log//base/Logging.h"
#include "Callbacks.h"
#include "Channel.h"
#include "TimerId.h"
#include "TimerQueue.h"

#include <sys/eventfd.h>

using namespace muduo;


__thread EventLoop* t_loopInThisThread = 0;
const int kPollTimeMs = 10000;

static int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}


EventLoop::EventLoop():looping_(false),threadId_(CurrentThread::tid()),
quit_(false),callingPendingFunctors(false),
poller_(new Poller(this)),
timerQueue_(new TimerQueue(this)),
wakeupFd_(createEventfd()),
wakeupChannel_(new Channel(this,wakeupFd_))
{

  LOG<<"Eventloop created"<<this<<"in thread"<<threadId_;
  if(t_loopInThisThread) {
    LOG<<"Another EventLoop"<<t_loopInThisThread
    <<"exists in this thread"<<threadId_;
  }else {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setWReadCallback(std::bind(&EventLoop::handleRead,this));
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
  assert(!looping_);
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

void EventLoop::runInLoop(const Functor& cb) {
  if(isInLoopThread()) {
    cb();
  }else {
    queueInLoop(cb);
  }
}

void EventLoop::queueInLoop(const Functor& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.push_back(cb);
  }
  if(!isInLoopThread()||callingPendingFunctors) {
    wakeup();
  }
}

void EventLoop::wakeup() {
  int one = 1;
  ssize_t n = ::write(wakeupFd_,&one,sizeof one);
  if(n!=sizeof one) {
    LOG<<"EventLoop::wakeup() writes"<<n<<" bytes instead of 8";
  }
}


void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false;

  while(!quit_) {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
    for(Poller::ChannelList::iterator it = activeChannels_.begin();it!=activeChannels_.end();++it) {
      (*it) -> handleEvent(pollReturnTime_);
    }
    dePendingFunctors();
  }
  LOG<<"EventLoop"<<this<<" stop looping";
  looping_ = false;
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

void EventLoop::quit(){
  quit_ = true;
  if(!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->removeChannel(channel);
}

TimerId EventLoop::runAt(const Timestamp& time,const TimerCallback& cb) {
  return timerQueue_->addTimer(cb,time,0.0);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb) {
      Timestamp time(addTime(Timestamp::now(),interval));
      return timerQueue_->addTimer(cb,time,interval);
}

TimerId EventLoop::runAfter(double delay,const TimerCallback& cb) {
  Timestamp time(addTime(Timestamp::now(),delay));
  return runAt(time,cb);
}

void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}


void EventLoop::dePendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  for(size_t i = 0;i<functors.size();i++) {
    functors[i]();
  }
  callingPendingFunctors = false;
}

void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG<< "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}











