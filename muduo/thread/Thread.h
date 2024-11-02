

#ifndef THREAD_H
#define THREAD_H
#include "Atomic.h"

#include <functional>
#include <memory>
#include <pthread.h>
#include <string>

namespace muduo
{

class Thread
{
public:
  using ThreadFunc = std::function<void()>;

  // 构造函数，接受线程函数和线程名称
  explicit Thread(const ThreadFunc&, const std::string& name = std::string());
  ~Thread();

  // 启动线程
  void start();
  // 等待线程结束
  void join();

  // 判断线程是否已启动
  bool started() const { return started_; }
  // 获取线程ID
  pid_t tid() const { return *tid_; }
  // 获取线程名称
  const std::string& name() const { return name_; }

  // 获取已创建的线程数量
  static int numCreated() { return numCreated_.get(); }

private:
  bool        started_;  // 线程是否已启动
  bool        joined_;   // 线程是否已加入
  pthread_t   pthreadId_;  // POSIX 线程ID
  std::shared_ptr<pid_t> tid_;  // 线程ID的智能指针
  ThreadFunc  func_;  // 线程执行的函数
  std::string name_;  // 线程名称

  static AtomicInt32 numCreated_;  // 创建的线程数量
};

namespace CurrentThread
{
pid_t tid();  // 获取当前线程的ID
const char* name();  // 获取当前线程的名称
bool isMainThread();  // 判断当前线程是否为主线程
}

}
#endif //THREAD_H
