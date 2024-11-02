

#ifndef EPOLL_H
#define EPOLL_H

#include <map>
#include <vector>

#include "TimeStamp.h"
#include "EventLoop.h"

struct epoll_event;

namespace muduo {

class Channel;

//使用epoll 实现的IO多路复用
//该类不拥有Channel对象的所有权

class EPoller {
public:
  typedef std::vector<Channel*> ChannelList;

  //构造函数，初始化事件循环和epoll文件描述符
  EPoller(EventLoop* loop);

  //析构函数
  ~EPoller();

  //轮询I/O事件，必须在循环线程中调用
  Timestamp poll(int timeoutMs,ChannelList* activeChannels);

  //更新感兴趣的I/O事件，必须在循环线程中调用
  void updateChannel(Channel* channel);

  //移除某个Channel_,通常在Channel_析构时调用，必须在循环线程中调用
  void removeChannel(Channel* channel);

  //确保操作在循环线程中进行
  void assertInLoopThread();

private:
  static const int kInitEventListSize = 16;

  //填充活跃的Channel的列表
  void fillActiveChannels(int numEvents,ChannelList* activeChannels) const;

  //更新epoll中的事件
  void update(int operation,Channel* channel_);

  //使用vector存储epoll_event和map存储文件描述符到Channel的映射
  typedef std::vector<struct epoll_event> EventList;
  typedef std::map<int,Channel*> ChannelMap;

  EventLoop* ownerLoop_; //拥有该EPoller的事件循环
  int epollfd_;  //epoll文件描述符
  EventList events_; //存储时间的列表
  ChannelMap channels_;  //存储每个文件描述符对应的Channel

};

}


#endif //EPOLL_H
