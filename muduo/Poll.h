

#ifndef POLL_H
#define POLL_H
#include <sys/poll.h>

#include <map>
#include <vector>

#include "EventLoop.h"
#include "TimeStamp.h"
struct pollfd;
namespace  muduo {
  class Channel;
  class EventLoop;


//IO Multiplexing with poll

class Poller {
public:
  typedef std::vector<Channel*> ChannelList;
  explicit Poller(EventLoop* loop);
  ~Poller();

  //Polls the IO events
  //Must be called in the loop thread
  Timestamp poll(int timeoutMs,ChannelList* activeChannels);

  //Changes the interested IO events
  //Must be called int the loop thread
  void updateChannel(Channel* channel);

  void removeChannel(Channel* channel);

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
