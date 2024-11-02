#Poller

Poller class 是IO multiplexing的封装，muduo同时支持poll和epoll两种IO multiplexing的机制。Poller是EventLoop的间接成员，只供其owner EventLoop在IO线程调用，因此无需加锁，其生命周期与EcentLoop相等。**Poller并不拥有Channel,Channel在析构之前必须自己unregistered(EventLoop::removeChannel()),避免空悬指针**



~~~C++

#ifndef POLL_H
#define POLL_H
#include <poll.h>

#include <vector>
#include <map>

#include "EventLoop.h"
struct pollfd;
namespace  muduo {
class Channel;
//IO Multiplexing with poll

class Poller {
public:
  typedef std::vector<Channel*> ChannelList;
  explicit Poller(EventLoop* loop);
  ~Poller();

  //Polls the IO events
  //Must be called in the loop thread
  
  //return typeof value is Timestamp
  int poll(int timeoutMs,ChannelList* activeChannels);

  //Changes the interested IO events
  //Must be called int the loop thread
  void updateChannel(Channel* channel);

  void assertInLoopThread() {

  }

private:
  void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;


  typedef std::vector<struct pollfd> PollFdList;
  typedef std::map<int ,Channel*> ChannelMap;

  EventLoop* ownerLoop_;
  PollFdList pollfds_;
  ChannelMap channels_;

};

}
#endif //POLL_H

~~~



ChannelMap是从fd到Channel*的映射。Poller::poll()不会在每次调用poll之前临时构造pollfd数组，而是把它缓存起来(pollfds_)







Poller::poll()是Poller的核心功能，它调用poll获得当前活动的IO事件，然后填充调用方传入的activeChannels,并返回poll return的时刻。这里我们直接把vector<struct pollfd> pollfds_作为参数传给poll,因为C++标准保证std::vector的元素排列跟数组一样



~~~C++

using namespace muduo;

muduo::Poller::Poller(EventLoop* loop) :ownerLoop_(loop){

}

muduo:: Poller::~Poller() = default;

TimerStamp Poller::poll(int timeoutMs,muduo::Poller::ChannelList* activeChannels) {
  int numEvents = ::poll(&*pollfds_.begin(),pollfds_.size(),timeoutMs);
  TimerStamp now;
  if(numEvents>0) {
    LOG<<numEvents<<" events happended";
    fillActiveChannels(numEvents,activeChannels);
  }else if(numEvents == 0) {
    LOG<<" nothing happended";
  }else {
    LOG<<"Poller::poll()";
  }
  return now;
}

void Poller::fillActiveChannels(int numEvents,
                                ChannelList* activeChannels) const {
for(PollFdList::const_iterator pfd = pollfds_.begin();pfd!=pollfds_.end()&&numEvents>0;++pfd) {
  if(pfd->revents>0) {
    --numEvents;
    ChannelMap::const_iterator ch = channels_.find(pfd->fd);
    assert(ch!=channels_.end());
    Channel* channel_thing = ch->second;
    assert(channel_thing->fd()==pfd->fd());
    channel_thing->set_revents(pfd->revents);
    activeChannels->push_back(channel_thing);
  }
}
 
}
~~~



fliiActiveChannels()遍历pollfds_,找出有活动事件的fd,把它对应的Channel填入activeChannels.这个函数的复杂度是O(N),其中N是文件描述符的数目，为了提前结束循环，没找到一个活动fd就递减numEvents,这样当numEvents减为0时表示活动fd都找完了，当前活动时间revents会保存在Channel中，供Channel::handleEent()使用

**不能一边遍历pollfds_,一边调用handleEvent**



Poller::updatChannnel()的主要功能是负责和维护更新pollfds_数组，添加新Channel的复杂度是O(1),因为Channnel记住了自己在pollfds_ _数组中的下标，因此可以快速定位

如果某个Channel暂时不关心任何事件，就把pollfd.fd设为-1，让poll(2)忽略此项

~~~~C++
void Poller::updateChannel(Channel* channel) {
  assertInLoopThread();
  LOG<<"fd = "<<channel->fd()<<"events = "<<channel->events();
  if(channel->index()<0) {
    assert(channels_.find(channel->fd())!=channels_.end());
    pollfd pfd{};
    pfd.fd = channel->fd();
    pfd.events  = channel->events();
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size())-1;
    channel->set_index(idx);
    channels_[pfd.fd] = channel;
  }else {
    assert(channels_.find(channel->fd())!=channels_.end());
    assert(channels_[channel->fd()] == channel);
    int idx = channel->index();
    assert(0<=idx&&idx<pollfds_.size());
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd ==channel->fd()||pfd.fd == -1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    if(channel->isNoneEvent()) {
      pfd.fd = -1;
    }
  }
}
~~~~



对EventLoop进行改动

~~~C++
  bool looping_;  //atomic
  bool quit_;
  std::shared_ptr<Poller> poller_;
  muduo::Poller::ChannelList activeChannels_;
  const pid_t threadId_;


void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false;
  while(!quit_) {
    activeChannels_.clear();
    poller_->poll(5,&activeChannels_);
    for(Poller::ChannelList::iterator it = activeChannels_.begin();it!=activeChannels_.end();++it) {
      (*it) -> handleEvent();
    }
  }
}
~~~

![Poll](/img/img_17.png)

