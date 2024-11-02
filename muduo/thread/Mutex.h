
#ifndef MUTEX_H
#define MUTEX_H

#include "Thread.h"

#include <cassert>
#include <pthread.h>

namespace muduo
{

class MutexLock
{
public:
  // 构造函数，初始化互斥锁，并将持有者设置为 0（未被任何线程持有）
  MutexLock()
    : holder_(0)
  {
    pthread_mutex_init(&mutex_, nullptr);
  }

  // 析构函数，销毁互斥锁，确保在销毁时没有任何线程持有该锁
  ~MutexLock()
  {
    assert(holder_ == 0);
    pthread_mutex_destroy(&mutex_);
  }

  // 判断当前线程是否持有该锁
  bool isLockedByThisThread()
  {
    return holder_ == CurrentThread::tid();
  }

  // 断言当前线程持有该锁
  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // 锁定互斥锁，并记录持有该锁的线程ID
  void lock()
  {
    pthread_mutex_lock(&mutex_);
    holder_ = CurrentThread::tid();
  }

  // 解锁互斥锁，并将持有者设置为 0
  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

  // 获取底层的 pthread 互斥锁对象
  pthread_mutex_t* getPthreadMutex()
  {
    return &mutex_;
  }

private:
  pthread_mutex_t mutex_;  // 底层的 pthread 互斥锁
  pid_t holder_;           // 当前持有锁的线程ID
};

class MutexLockGuard
{
public:
  // 构造函数，锁定传入的互斥锁
  explicit MutexLockGuard(MutexLock& mutex) : mutex_(mutex)
  {
    mutex_.lock();
  }


  // 析构函数，解锁互斥锁
  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

private:
  MutexLock& mutex_;  // 引用一个互斥锁对象
};

}

// 防止错误用法，比如: MutexLockGuard(mutex_);
// 临时对象无法长时间持有锁，必须为该类命名一个对象
#define MutexLockGuard(x) error "Missing guard object name"

#endif //MUTEX_H
