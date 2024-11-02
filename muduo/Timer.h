

#ifndef TIMER_H
#define TIMER_H
#include <cstdint>             // 用于 int64_t 类型
#include <functional>          // 用于 std::function
#include <utility>
#include "Timestamp.h" // 时间戳类
#include "thread/Atomic.h"     // 自定义的原子计数类
#include "Callbacks.h"         // 定义 TimerCallback 类型

namespace muduo
{

///
/// 用于表示计时器事件的内部类。
///
class Timer
{
public:
  /// 构造函数
  /// @param cb 定时器回调函数
  /// @param when 定时器到期时间
  /// @param interval 重复间隔，0 表示不重复
  Timer(TimerCallback  cb, Timestamp when, double interval)
    : callback_(std::move(cb)),           // 初始化回调函数
      expiration_(when),       // 初始化到期时间
      interval_(interval),     // 初始化间隔时间
      repeat_(interval > 0.0), // 判断是否重复
      sequence_(s_numCreated_.incrementAndGet()) // 获取唯一的计时器序列号
  {
  }

  /// 运行定时器的回调函数
  void run() const
  {
    callback_();
  }

  /// 获取定时器到期时间
  Timestamp expiration() const  { return expiration_; }

  /// 判断定时器是否重复
  bool repeat() const { return repeat_; }

  /// 获取计时器的序列号
  int64_t sequence() const { return sequence_; }

  /// 重启定时器
  /// @param now 当前时间，用于重新计算到期时间
  void restart(Timestamp now);

private:
  const TimerCallback callback_;   // 定时器回调函数
  Timestamp expiration_;           // 定时器到期时间
  const double interval_;          // 定时器间隔时间
  const bool repeat_;              // 是否重复
  const int64_t sequence_;         // 定时器唯一序列号

  static AtomicInt64 s_numCreated_; // 计时器创建数量的全局计数器
};

}
#endif //TIMER_H
