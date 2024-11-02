

#ifndef EVENTLOOPTHREADPOOL_H
#define EVENTLOOPTHREADPOOL_H




#include <vector>

#include <memory>



namespace muduo {
class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
public:
  EventLoopThreadPool(EventLoop * baseLoop);
  ~EventLoopThreadPool();
  void start();
  void setThreadNum(int numThreads){}
  EventLoop* getNextLoop();

private:
  EventLoop* baseLoop_;
  bool started_;
  int numThreads_;
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop*> loops_;


};


}

#endif //EVENTLOOPTHREADPOOL_H
