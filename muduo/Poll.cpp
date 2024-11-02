
#include "Poll.h"

#include "../muduo/log/base/Logging.h"
#include "Channel.h"
#include "TimeStamp.h"
#include <functional>
using namespace muduo;

muduo::Poller::Poller(EventLoop* loop) :ownerLoop_(loop){}

muduo:: Poller::~Poller() = default;

Timestamp Poller::poll(int timeoutMs,muduo::Poller::ChannelList* activeChannels) {
  int numEvents = ::poll(&*pollfds_.begin(),pollfds_.size(),timeoutMs);
  Timestamp now;
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
for(auto pfd = pollfds_.begin();pfd!=pollfds_.end()&&numEvents>0;++pfd) {
  if(pfd->revents>0) {
    --numEvents;
    auto ch = channels_.find(pfd->fd);
    assert(ch!=channels_.end());
    Channel* channel_thing = ch->second;
    assert(channel_thing->fd()==pfd->fd);
    channel_thing->set_revents(pfd->revents);
    activeChannels->push_back(channel_thing);
  }
}
}


void Poller::updateChannel(Channel* channel)
{
  assertInLoopThread();
  LOG << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0) {
    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size())-1;
    channel->set_index(idx);
    channels_[pfd.fd] = channel;
  } else {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    if (channel->isNoneEvent()) {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;
    }
  }
}

void Poller::removeChannel(Channel* channel)
{
  assertInLoopThread();
  LOG << "fd = " << channel->fd();

  // 确保 channel 对应的文件描述符在 channels_ 中存在
  assert(channels_.find(channel->fd()) != channels_.end());
  // 确保存储的 channel 和传入的是同一个对象
  assert(channels_[channel->fd()] == channel);
  // 确保该 channel 没有需要监听的事件
  assert(channel->isNoneEvent());

  int idx = channel->index();
  // 确保 idx 合法，且在 pollfds_ 的范围内
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));

  // 验证 pollfds_ 中该位置的文件描述符和事件是否匹配
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());

  // 从 channels_ 中移除该 channel
  size_t n = channels_.erase(channel->fd());
  assert(n == 1); (void)n;

  // 如果要移除的 channel 是 pollfds_ 的最后一个元素，直接弹出
  if (static_cast<size_t>(idx) == pollfds_.size() - 1) {
    pollfds_.pop_back();
  } else {
    // 如果不是最后一个元素，将其与最后一个元素交换
    int channelAtEnd = pollfds_.back().fd;
    std::iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);

    // 调整交换后的 channel 在 channels_ 中的位置索引
    if (channelAtEnd < 0) {
      channelAtEnd = -channelAtEnd - 1;
    }
    channels_[channelAtEnd]->set_index(idx);
    // 移除最后一个元素
    pollfds_.pop_back();
  }
}
