

#ifndef TIMERID_H
#define TIMERID_H
namespace muduo {

class Timer;

class TimerId {
public:
  explicit TimerId(Timer* timer = nullptr,int64_t seq = 0):timer_(timer),sequence_(seq){}
  friend class TimerQueue;

private:
  Timer* timer_;
  int64_t sequence_;
};
}
#endif //TIMERID_H
