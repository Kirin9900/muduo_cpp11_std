

#include "Thread.h"

#include <memory>
#include <unistd.h>
#include <sys/types.h>
#include <cassert>
#include <cstring>
#include <iostream>

#if __FreeBSD__
#include <pthread_np.h>
#else
#include <sys/prctl.h>
#include <linux/unistd.h>
#endif

namespace muduo
{
namespace CurrentThread
{
  __thread const char* t_threadName = "unnamedThread";  // 当前线程名称
}
}

namespace
{
__thread pid_t t_cachedTid = 0;  // 缓存的线程ID

#if __FreeBSD__
pid_t gettid()
{
  return pthread_getthreadid_np();
}
#else
#if !__GLIBC_PREREQ(2,30)
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}
#endif
#endif

// fork后更新线程信息
void afterFork()
{
  t_cachedTid = gettid();
  muduo::CurrentThread::t_threadName = "main";
}

// 线程名称初始化器，用于主线程
class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

ThreadNameInitializer init;  // 全局实例，确保主线程名称初始化

struct ThreadData
{
  using ThreadFunc = muduo::Thread::ThreadFunc;
  ThreadFunc func_;  // 线程执行的函数
  std::string name_;  // 线程名称
  std::weak_ptr<pid_t> wkTid_;  // 线程ID的弱指针

  ThreadData(const ThreadFunc& func, const std::string& name, const std::shared_ptr<pid_t>& tid)
    : func_(func),
      name_(name),
      wkTid_(tid)
  { }

  // 在新线程中运行
  void runInThread()
  {
    pid_t tid = muduo::CurrentThread::tid();
    auto ptid = wkTid_.lock();

    if (ptid)
    {
      *ptid = tid;
    }

    if (!name_.empty())
      muduo::CurrentThread::t_threadName = name_.c_str();
#if __FreeBSD__
    pthread_setname_np(pthread_self(), muduo::CurrentThread::t_threadName);
#else
    ::prctl(PR_SET_NAME, muduo::CurrentThread::t_threadName);
#endif
    func_();  // 执行线程函数
    muduo::CurrentThread::t_threadName = "finished";
  }
};

// 线程启动函数
void* startThread(void* obj)
{
  ThreadData* data = static_cast<ThreadData*>(obj);
  data->runInThread();
  delete data;
  return nullptr;
}

}

using namespace muduo;

pid_t CurrentThread::tid()
{
  if (t_cachedTid == 0)
  {
    t_cachedTid = gettid();
  }
  return t_cachedTid;
}

const char* CurrentThread::name()
{
  return t_threadName;
}

bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

AtomicInt32 Thread::numCreated_;

Thread::Thread(const ThreadFunc& func, const std::string& n)
  : started_(false),
    joined_(false),
    pthreadId_(0),
    tid_(std::make_shared<pid_t>(0)),
    func_(func),
    name_(n)
{
  numCreated_.increment();
}

Thread::~Thread()
{
  if (started_ && !joined_)
  {
    pthread_detach(pthreadId_);
  }
}

void Thread::start()
{
  assert(!started_);  // 确保线程尚未启动，防止重复调用
  started_ = true;    // 标记线程已启动

  // 创建线程数据，将线程函数、线程名称和线程ID指针传入
  auto data = new ThreadData(func_, name_, tid_);

  // 使用 pthread_create 创建一个新的线程，并传递启动函数和数据指针
  if (pthread_create(&pthreadId_, nullptr, &startThread, data))
  {
    started_ = false; // 如果创建失败，重置 started_ 标志
    delete data;      // 删除分配的线程数据
    abort();          // 终止程序，因为创建线程失败
  }
}

void Thread::join()
{
  assert(started_);
  assert(!joined_);
  joined_ = true;
  pthread_join(pthreadId_, nullptr);
}