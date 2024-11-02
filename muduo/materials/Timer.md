## 定时器



### 程序中的时间

在服务端程序设计中，与时间有关的常见任务有：

1. 获取当前时间，计算时间间隔
2. 时区转换与日期计算
3. **定时操作，比如在预定的时间执行任务，或者在一段延时之后执行任务**



### Linux时间函数

Linux的计时函数，用于获取当前时间:

time  /  time_t(秒)

ftime / struct timeb(毫秒)

**gettimeofday/ struct timeval(毫秒)**

Clock_gettime / struct timespec(纳秒)



定时函数  用于让程序等待一段时间或安排计划任务：

sleep

alarm

usleep

nanosleep

Clock_nanosleep

gettimer/settimer

timer_create/timer_settime/time_gettimer/timer_delete

**timerfd_create/timerfd_gettime/timerfd_settime**



计时只使用gettimeofday来获取当前时间

定时只使用timer_fd*系列函数来处理定时任务



gettimeofday入选原因：

1.time的精度太低，ftime已经被废弃，clock_gettime精度最高，但是其系统调用的开销比gettimeofday大

2  gettimeofday不是系统调用，而是在用户态实现的，没有上下文切换和陷入内核的开销

3 gettimeofday的分辨率是1微秒，现在的实现确实能够满足日常计时的需求



timerfd_*入选的原因:

Sleep/alarm/usleep在实现时有可能用了SIGALRM信号，**在多线程程序中处理信号是个相当麻烦的事情，应当尽量避免**

nanosleep和clock_nanosleep是线程安全的，但是在非阻塞网络编程中，绝对不能以线程挂起的方式等待一段时间，这样一来程序会失去响应

gettimer和timer_create也是用信号来deliver超时

**Timerfd_create把时间变成文件描述符，该文件在定时器超时的那一刻变得可读**，这样就能方便融入select/poll对的框架中，用统一的方式来处理IO事件和超时时间

poll和 epoll_wait的定时精度只有毫秒，远低于timerfd_settime的定时精度



### muduo的定时器接口

muduo EventLoop有三个定时函数

~~~C++
typedef boost::function<void()> TimerCallback;
class EventLoop: boost noncopyable{
  public:
  
  TimerId runAt(const TimeStamp& time,const TimerCallback& cb);
  
  TimerId runEvery(double interval,const TimeCallback& cb);
  
  TimerId runAfter(double delay, const TimerCallback& cb);
  
  void cancel(TimerId timerId)l
}
~~~

runAt  在**指定的时间**调用TimerCallback;

runAfter  等一段时间调用TimerCallback;

runEvery  以固定的间隔反复调用TimerCallback



回调函数在EventLoop对象所属的线程发生，与onMessage，onConnection等网络事件函数在同一个线程。muduo的TimerQueue采用了平衡二叉树来管理未到期的timers，时间复杂度为O(log N)