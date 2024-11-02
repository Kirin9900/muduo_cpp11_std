

#ifndef CHANNEL_H
#define CHANNEL_H
#include <functional>
#include "TimeStamp.h"
namespace muduo {
class EventLoop;

class Channel {
public:
  typedef std::function<void ()> EventCallback;
  using EventReadCallback = std::function<void(Timestamp)>;

  Channel(EventLoop* loop,int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);
  void setWriteCallback(const EventCallback& cb) {
    writeCallback_ = cb;
  }
  void setWReadCallback(const  EventReadCallback & cb) {
    readCallback_ = cb;
  }
  void setErrorCallback(const EventCallback& cb) {
    errorCallback_ = cb;
  }

  void setCloseCallback(const EventCallback& cb) {
    closeCallback_ = cb;
  }
  int fd() const{return fd_;}
  int events() const{return events_;}
  void set_revents(int revt){revents_ = revt;}
  bool isNoneEvent()const{return events_ == kNoneEvent;}

  //启动
  void enableReading(){events_|=kReadEvent;update();}
  void enableWriting(){events_|=kWriteEvent;update();}
  void disableWriting(){events_&= ~kWriteEvent;update();}
  void disableAll(){events_= kNoneEvent;update();}
  bool isWriting() const {return events_ & kWriteEvent;}



  //for Poller
  int index() const{return index_;}
  void set_index(int idx){index_ = idx;}

  EventLoop* ownerLoop() {
    return loop_;
  }
private:
  void update();

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;
  const int fd_;
  int events_;
  int revents_;
  int index_;

  bool eventHandling_;

  EventCallback writeCallback_;
  EventCallback errorCallback_;
  EventReadCallback  readCallback_;
  EventCallback closeCallback_;

};
}
#endif //CHANNEL_H
