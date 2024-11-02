
#include "Channel.h"

#include "sys/poll.h"


#include "log//base/Logging.h"

#include "EventLoop.h"
#include <poll.h>



using namespace muduo;

const int muduo::Channel::kNoneEvent = 0;
const int muduo::Channel::kReadEvent = POLLIN/POLLPRI;
const int muduo::Channel::kWriteEvent = POLLOUT;

muduo::Channel::Channel(muduo::EventLoop* loop,int fdArg)
  :loop_(loop),fd_(fdArg),events_(0),revents_(0),index_(-1),eventHandling_(false)
{
}

muduo::Channel::~Channel() {
  assert(!eventHandling_);
}


void muduo::Channel::update() {
  loop_->updateChannel(this);
}

void muduo::Channel::handleEvent(Timestamp receiveTime) {
  eventHandling_ = true;
  if(revents_& POLLNVAL) {
    LOG<<"Channel::handel_event() POLLNVAL";
  }
  if(revents_& (POLLERR|POLLNVAL)) {
    if(errorCallback_) errorCallback_();
  }

  //POLLHUP 表示挂起事件，即对方关闭了连接或连接断开。
  if((revents_& POLLHUP)&&!(revents_&POLLIN)) {
    LOG<<"Channel::handle_event() POLLHUP";
    if(closeCallback_) closeCallback_();
  }

  if(revents_&(POLLIN|POLLPRI/POLLRDHUP)) {
    if(readCallback_){return readCallback_(receiveTime);}
  }
  if(revents_&(POLLOUT)) {
    if(writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}
