

#ifndef CONDITION_H
#define CONDITION_H

#include "Mutex.h"

#include <pthread.h>
#include <cerrno>

namespace muduo
{

class Condition
{
public:
  // 构造函数，初始化条件变量
  explicit Condition(MutexLock& mutex) : mutex_(mutex)
  {
    pthread_cond_init(&pcond_, nullptr);
  }

  // 析构函数，销毁条件变量
  ~Condition()
  {
    pthread_cond_destroy(&pcond_);
  }

  // 等待条件变量，该函数会释放互斥锁并挂起线程，直到被唤醒
  void wait()
  {
    pthread_cond_wait(&pcond_, mutex_.getPthreadMutex());
  }

  // 带超时功能的等待，返回 true 表示超时，false 表示被正常唤醒
  bool waitForSeconds(int seconds)
  {
    struct timespec abstime;
    clock_gettime(CLOCK_REALTIME, &abstime);
    abstime.tv_sec += seconds;
    return ETIMEDOUT == pthread_cond_timedwait(&pcond_, mutex_.getPthreadMutex(), &abstime);
  }

  // 唤醒一个等待该条件变量的线程
  void notify()
  {
    pthread_cond_signal(&pcond_);
  }

  // 唤醒所有等待该条件变量的线程
  void notifyAll()
  {
    pthread_cond_broadcast(&pcond_);
  }

private:
  MutexLock& mutex_;         // 引用一个互斥锁对象
  pthread_cond_t pcond_;     // pthread 条件变量
};

}

#endif //CONDITION_H
