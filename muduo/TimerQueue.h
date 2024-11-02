
#ifndef TIMERQUEUE_H
#define TIMERQUEUE_H
#include <set>
#include <vector>
#include <memory>       // 使用智能指针管理对象

#include "Timestamp.h"
#include "thread/Mutex.h"
#include "Callbacks.h"
#include "Channel.h"

namespace muduo
{

class EventLoop;
class Timer;
class TimerId;


class TimerQueue : public std::enable_shared_from_this<TimerQueue>
{
public:
  // 构造函数，初始化事件循环
  TimerQueue(EventLoop* loop);

  // 析构函数，清理资源
  ~TimerQueue();

  ///
  /// 添加定时器，将回调函数设定在指定时间执行
  /// interval > 0.0 时表示重复执行
  /// 线程安全，通常被其他线程调用
  ///
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  // 取消定时器，安全取消
  void cancel(TimerId timerId);

private:
  // 定义 TimerList 类型，表示按到期时间排序的定时器集合
  using Entry= std::pair<Timestamp, Timer *>;
  using TimerList = std::set<Entry>;

  // 定义 ActiveTimerSet 类型，用于管理活动定时器的集合
  using ActiveTimer = std::pair<Timer*, int64_t>;
  using ActiveTimerSet = std::set<ActiveTimer>;

  // 在事件循环中添加定时器
  void addTimerInLoop(Timer* timer);

  // 在事件循环中取消定时器
  void cancelInLoop(TimerId timerId);

  // 处理读事件，处理定时器到期
  void handleRead();

  // 获取已到期的所有定时器
  std::vector<Entry> getExpired(Timestamp now);

  // 重置定时器队列，将到期定时器重新添加
  void reset(const std::vector<Entry>& expired, Timestamp now);

  // 插入定时器，并返回是否更改最早到期的定时器
  bool insert(Timer* timer);

  EventLoop* loop_;         // 事件循环指针
  int timerfd_;             // 定时器文件描述符
  Channel timerfdChannel_;  // 关联的通道对象

  TimerList timers_;        // 按过期时间排序的定时器列表
  bool callingExpiredTimers_; // 表示当前是否正在调用到期的定时器
  ActiveTimerSet activeTimers_; // 活动定时器集合
  ActiveTimerSet cancelingTimers_; // 正在取消的定时器集合
};

}
#endif //TIMERQUEUE_H
