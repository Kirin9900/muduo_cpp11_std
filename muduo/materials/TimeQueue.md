## TimerQueue定时器

muduo定时器的功能由三个class实现，TimerId,Timer,TimerQueue,用户只能看到第一个class,另外两个都是内部实现细节

TimerQueue数据结构的选择，TimerQueue需要高效组织尚未到期的Timer，能够快速地根据当前时间找到已经到期的Timer，也要能高效地添加和删除Timer

在最新的版本中，使用的是二叉搜索树(std::set/std::map)，我们不能直接用map<Timestamp,Timer*)，因为这样无法处理两个Timer到期时间相同的情况，所以我们使用pair<Timestap,Timer*>作为key,这样即使两个Timer的到期时间相同，它们的地址也必定不同






### Timestamp




Timestamp用于表示基于微秒(microseconds)的时间戳

Timestamp是一个不可变类，表示时间戳，精度为微秒，

成员函数

void swap 交换两个Timestamp的对象的microSecondsSinceEpoch_值，

toString() 将时间戳转为字符串

bool vaild() const: 判断时间戳是否有效(微妙数>0)

microSecondsSinceEpoch() const 获取自Unix纪元以来的微妙数

static Timestamp now()  获取当前时间的Timestamp对象

static Timestamp invalid()  返回一个无效的TImestamp对象

static const int kMicroSecondsPerSecond:常量，每秒的微妙数

inline double timeDifference(Timestamp high,Timestamp low)  计算两个时间戳之间的差值，单位是秒

inline Timestamp addTime(Timestamp timestamp,double seconds) 将指定的秒数(seconds* 1000)加到timestamp上，返回新的Timestamp对象  

[TimeStamp.h](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/TimeStamp.h)  

[TimeStamp.cc](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/TimeStamp.cpp)


### Timer
初始化Timer对象

cb: 定时器出发时调用的回调函数

when： 定时器的初始到期时间

interval_ : 定时器的重复时间间隔，单位为秒

repeat_ : bool类型，表示是否是重复定时器

成员函数

void run() const 执行定时七的回调函数，即触发定时器调用callback_

Timerstamp expiration() const ：获取定时器的到期时间

bool repeat() const :判断定时器是否是重复定时器

void restart(Timestamp now) 重新启动定时器，如果是是重复定时器，将到期时间设置为now+interval_ 如果不是，则设置为无效时间

void Timer::restart(Timestamp now) 根据定时器是否重复来重置到期时间

如果repeat_为true,则使用addTime()函数，将当前时间now+interval，并将结果赋给expiration_

如果repeat_为false，则将expiration设置为无效时间戳Timestamp::invalid()

[Timer.h](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/Timer.h)
[Timer.cc](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/Timer.cpp)  





### TimeQueue

[TimerQueue.h](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/TimerQueue.h)  

[TimerQueue.cc](https://github.com/Kirin9900/muduo_cpp11_std/blob/main/muduo/TimerQueue.cpp)  




`TimerQueue` 类用于管理多个 `Timer` 对象的触发和处理。它使用 `timerfd`（Linux 提供的文件描述符）来触发定时器事件，并结合 `EventLoop` 在事件到达时进行处理。

**细节解释**：

1. **`createTimerfd()` 函数**:
   ```cpp
   int createTimerfd()
   {
     int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
     if (timerfd < 0)
     {
       LOG_SYSFATAL << "Failed in timerfd_create";
     }
     return timerfd;
   }
   ```
    - 使用 `timerfd_create` 创建一个 `timerfd`，这是一个定时器文件描述符。
    - `CLOCK_MONOTONIC`：时间基于系统启动时间（单调递增）。
    - `TFD_NONBLOCK`：非阻塞模式，读操作不会阻塞。
    - `TFD_CLOEXEC`：在执行 `exec` 时关闭 `timerfd`，防止文件描述符泄露。
    - 如果 `timerfd_create` 返回的值小于 0，表示创建失败，记录日志。

2. **`howMuchTimeFromNow()` 函数**:
   ```cpp
   struct timespec howMuchTimeFromNow(Timestamp when)
   {
     int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
     if (microseconds < 100)
     {
       microseconds = 100;
     }
     struct timespec ts;
     ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
     ts.tv_nsec = static_cast<long>((microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
     return ts;
   }
   ```
    - 计算定时器到期时间与当前时间的差值，并返回 `timespec` 结构（秒和纳秒）。
    - 如果时间差小于 100 微秒，为了防止时间过短，强制设为 100 微秒。

3. **`readTimerfd()` 函数**:
   ```cpp
   void readTimerfd(int timerfd, Timestamp now)
   {
     uint64_t howmany;
     ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
     LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
     if (n != sizeof howmany)
     {
       LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
     }
   }
   ```
    - 从 `timerfd` 读取数据，确定触发了多少次定时事件（虽然每次读取都是 8 字节）。
    - `howmany` 表示有多少次定时器超时。
    - 日志记录读取的字节数。

4. **`resetTimerfd()` 函数**:
   ```cpp
   void resetTimerfd(int timerfd, Timestamp expiration)
   {
     struct itimerspec newValue;
     bzero(&newValue, sizeof newValue);
     newValue.it_value = howMuchTimeFromNow(expiration);
     int ret = ::timerfd_settime(timerfd, 0, &newValue, nullptr);
     if (ret)
     {
       LOG_SYSERR << "timerfd_settime()";
     }
   }
   ```
    - 使用 `timerfd_settime` 函数设置 `timerfd` 的到期时间。
    - 计算距离到期的时间，并将其设置为 `newValue.it_value`。
    - 传递 `0` 表示相对时间。

5. **`TimerQueue::addTimer()` 函数**:
   ```cpp
   TimerId TimerQueue::addTimer(const TimerCallback& cb, Timestamp when, double interval)
   {
     Timer* timer = new Timer(cb, when, interval);
     loop_->assertInLoopThread();
     bool earliestChanged = insert(timer);
     if (earliestChanged)
     {
       resetTimerfd(timerfd_, timer->expiration());
     }
     return TimerId(timer);
   }
   ```
    - 创建一个新的 `Timer` 对象，注册回调和到期时间。
    - 如果插入的 `Timer` 是最早到期的，则更新 `timerfd` 的到期时间。
    - 返回 `TimerId` 用于标识该 `Timer`。

6. **`TimerQueue::handleRead()` 函数**:
   ```cpp
   void TimerQueue::handleRead()
   {
     loop_->assertInLoopThread();
     Timestamp now(Timestamp::now());
     readTimerfd(timerfd_, now);

     std::vector<Entry> expired = getExpired(now);

     for (auto& entry : expired)
     {
       entry.second->run();
     }

     reset(expired, now);
   }
   ```
    - 在定时器触发时被调用，读取 `timerfd`，获取当前时间。
    - 调用 `getExpired()` 获取已到期的定时器，并逐一执行它们的回调函数。
    - 调用 `reset()` 重新设置需要继续触发的定时器。

7. **`TimerQueue::getExpired()` 函数**:
   ```cpp
   std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
   {
     std::vector<Entry> expired;
     Entry sentry = std::make_pair(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
     TimerList::iterator it = timers_.lower_bound(sentry);
     std::copy(timers_.begin(), it, back_inserter(expired));
     timers_.erase(timers_.begin(), it);

     return expired;
   }
   ```
    - 获取所有在 `now` 之前到期的 `Timer` 对象。
    - 创建一个哨兵值 `sentry`，用于寻找到期的定时器。
    - 将到期的定时器复制到 `expired` 列表，并从 `timers_` 中删除。

8. **`TimerQueue::reset()` 函数**:
   ```cpp
   void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
   {
     Timestamp nextExpire;

     for (auto& entry : expired)
     {
       if (entry.second->repeat())
       {
         entry.second->restart(now);
         insert(entry.second);
       }
       else
       {
         delete entry.second;
       }
     }

     if (!timers_.empty())
     {
       nextExpire = timers_.begin()->second->expiration();
     }

     if (nextExpire.valid())
     {
       resetTimerfd(timerfd_, nextExpire);
     }
   }
   ```
    - 重置已到期的定时器。
    - 如果定时器是重复的，则重新启动并插入队列。
    - 否则，删除该定时器。
    - 更新下一个到期时间。

9. **`TimerQueue::insert()` 函数**:
   ```cpp
   bool TimerQueue::insert(Timer* timer)
   {
     bool earliestChanged = false;
     Timestamp when = timer->expiration();
     TimerList::iterator it = timers_.begin();
     if (it == timers_.end() || when < it->first)
     {
       earliestChanged = true;
     }
     timers_.insert(std::make_pair(when, timer));
     return earliestChanged;
   }
   ```
    - 将新的定时器插入到 `timers_` 集合中。
    - 如果插入的定时器是最早到期的，则 `earliestChanged` 设置为 `true`。

通过以上修改和解释，这段代码可以实现定时器队列管理，并且不再依赖 `boost`，而是使用 C++11 标准库进行回调绑定和内存管理。



