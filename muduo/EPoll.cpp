#include "EPoll.h"

#include <poll.h>
#include <sys/epoll.h>

#include <cassert>
#include <cerrno>
#include <ostream>

#include "../muduo/log//base/Logging.h"
#include "Channel.h"
#include <bits/poll.h>
using namespace muduo;

static_assert(EPOLLIN == POLLIN, "EPOLLIN and POLLIN constants must match");
static_assert(EPOLLPRI == POLLPRI, "EPOLLPRI and POLLPRI constants must match");
static_assert(EPOLLOUT == POLLOUT, "EPOLLOUT and POLLOUT constants must match");
static_assert(EPOLLRDHUP == POLLRDHUP, "EPOLLRDHUP and POLLRDHUP constants must match");
static_assert(EPOLLERR == POLLERR, "EPOLLERR and POLLERR constants must match");
static_assert(EPOLLHUP == POLLHUP, "EPOLLHUP and POLLHUP constants must match");


const int kNew = -1;
const int kAdded = 1;
const int kDeleted  = 2;

//构造函数，创建epoll实例，并分配初始事件列表大小
 EPoller::EPoller(EventLoop* loop)
   :ownerLoop_(loop),
    epollfd_(::epoll_create(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)
{
   if(epollfd_ < 0) {
     LOG << "EPoller::EPoller()";
   }
}

//析构函数：关闭epoll文件描述符
 EPoller::~EPoller()
{
   ::close(epollfd_);
}

//轮询IO事件，并将活跃的事件填充到activeChannels列表中

Timestamp EPoller::poll(int timeoutMs,ChannelList* activeChannels)
{
   int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
   Timestamp now = Timestamp::now();
   if(numEvents < 0) {
     LOG<<numEvents<<"events happened";
     fillActiveChannels(numEvents,activeChannels);
     if(static_cast<size_t>(numEvents) == events_.size()) {
       events_.resize(events_.size() * 2);
     }
   }else if(numEvents == 0) {
     LOG<<" nothing happened";
   }else {
     LOG<<"EPoller::poll()";
   }
   return now;
}

//将活跃事件填充到activeChannels列表中
void EPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels) const
{
   assert(static_cast<size_t>(numEvents)<events_.size());
   for(int i=0;i<numEvents;++i) {
     Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
     channel -> set_revents(events_[i].events); //设置事件
     activeChannels->push_back(channel);
   }
 }

//更新epoll中的Channel，根据需要添加，修改或删除十斤啊

void EPoller::updateChannel(Channel* channel)
{
   assertInLoopThread();
   const int index = channel->index();
   if(index == kNew || index == kDeleted) {
     int fd = channel->fd();
     if(index  == kNew) {
       channels_[fd] = channel;
     }
     channel->set_index(kAdded);
     update(EPOLL_CTL_ADD,channel);
   }else {
     int fd  = channel->fd();
     if(channel->isNoneEvent()) {
       update(EPOLL_CTL_DEL, channel);
       channel->set_index(kDeleted);
     }else {
       update(EPOLL_CTL_MOD,channel);
     }
   }
}

//移除epoll中的Channel
void EPoller::removeChannel(Channel* channel){
   assertInLoopThread();
   int fd = channel->fd();
   channels_.erase(fd);

   if(channel->index() == kAdded) {
     update(EPOLL_CTL_DEL,channel);
   }
   channel->set_index(kNew);
}

//执行epoll的事件更新操作(添加，修改或删除)
void EPoller::update(int operation,Channel* channel) {
   struct epoll_event event;
   bzero(&event,sizeof event);
   event.events = channel->events();
   event.data.ptr = channel;
   int fd  = channel->fd();
   if(::epoll_ctl(epollfd_,operation,fd,&event)<0) {
     if(operation == EPOLL_CTL_DEL) {
       if(operation == EPOLL_CTL_DEL) {
         LOG<<"epoll_ctl op=" << operation << " fd=" << fd;
       }else {
         LOG<<"epoll_ctl op=" << operation << " fd=" << fd;
       }
     }
   }
 }







