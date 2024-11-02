# EventLoop::runInLoop()

runInLoop的功能是：如果调用runInLoop的线程和EventLoop初始化时绑定的线程是同一个线程，那么可以直接执行回调函数cb，否则，就调用queueInLoop把cb加入到待执行队列（即pendingFunctors_）中。

**runInLoop的功能是：如果调用runInLoop的线程和EventLoop初始化时绑定的线程是同一个线程，那么可以直接执行回调函数cb，否则，就调用**`queueInLoop`**把cb加入到待执行队列（即`pendingFunctors_`）中。**



~~~c++
void EventLoop::runInLoop(const Functor& cb) {
  if(isInLoopThread()) {
    cb();
  }else {
    queueInLoop(cb);
  }
}
~~~

有了这个功能，我们就能在线程间调配执行任务

由于IO线程平时阻塞在事件循环EventLoop::loop()的poll调用中，为了让IO线程能立刻执行用户回调，我们需要设法唤醒它。我们可以使用Linux的eventfd,可以更高效的唤醒，它不必管理缓冲区



~~~C++
private:
  void abortNotInLoopThread();
  void handleRead(); //wake up
  void dePendingFunctors();

  typedef std::vector<Channel*> ChannelList;

  
  
  bool looping_;  //atomic
  bool quit_;
  bool callingPendingFunctors; //atomic
  const pid_t threadId_;
  Timestamp pollReturnTime_;

  
  int wakeupFd_;
  std::shared_ptr<Channel> wakeupChannel_;
  
  std::shared_ptr<Poller> poller_;

  ChannelList activeChannels_;
  
  std::shared_ptr<TimerQueue> timerQueue_;

  std::vector<Functor> pendingFunctors_;
  MutexLock mutex_;
~~~

wakeupChannel_用于处理wakeupFd_上的readable事件，将事件分发到handleRead()函数。其中只要pendingFunctors_暴露给了其他线程，因此用mutex_进行保护

queueInLoop()的实现很简单，将cb加入队列，并在必要时唤醒IO线程

~~~C++
void EventLoop::queueInLoop(const Functor& cb) {
  {
    MutexLockGuard lock(mutex_);
    pendingFunctors_.push_back(cb);
  }
  if(!isInLoopThread()||callingPendingFunctors) {
    wakeup();
  }
}
~~~

必要时有两种情况，如果调用queueInLoop()的线程不是IO线程，那么唤醒是必需的，如果在IO线程调用queueInLoop()，而此时正在调用pending  functor,那么也必需唤醒，换句话说，只有在IO线程的事件回调中调用queueInLoop()才无须wakeup()



#### createEventfd

~~~C++
static int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG << "Failed in eventfd";
    abort();
  }
  return evtfd;
}
~~~

​	•	**定义与功能**：

​	•	createEventfd() 函数调用了 Linux 系统的 eventfd 函数，用于创建一个事件通知文件描述符（eventfd）。该函数返回创建的文件描述符。

​	•	eventfd 是一种内核机制，可以在进程间或线程间传递事件。它的初始值是 0，**当该文件描述符被写入时，值会增加，被读取时会重置。**

​	•	**参数说明**：

​	•	0: 初始值，表示 eventfd 的计数器从 0 开始。

​	•	EFD_NONBLOCK: 非阻塞模式，使得对 eventfd 的读写操作不会阻塞进程或线程。

​	•	EFD_CLOEXEC: 当执行 exec 系列函数时自动关闭该文件描述符。

​	•	**错误处理**：

​	•	如果 eventfd 创建失败（即 evtfd < 0），则通过日志记录错误信息，并调用 abort() 终止程序。



#### Wakeup

~~~C++
void EventLoop::wakeup()
{
  int one = 1;
  ssize_t n = ::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}
~~~

​	•	**定义与功能**：

​	•	wakeup() 函数用于唤醒 EventLoop 对象所在的线程。

​	•	它通过向 wakeupFd_ 写入数据，触发 eventfd 事件，以便让 poll() 调用立即返回，从而处理新加入的任务。

​	•	**详细流程**：

​	•	创建一个 int one = 1 的变量，并使用 ::write() 向 wakeupFd_ 写入这个值。

​		•	**由于 wakeupFd_ 是一个 eventfd 文件描述符，写入操作会增加其内部计数器，从而触发 eventfd 的可读事件，使 EventLoop 中的 poll() 立即返回。**

​	•	如果写入的字节数与 sizeof one 不一致（即没有成功写入），则通过日志记录实际写入的字节数。



~~~C++
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG<< "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}
~~~



#### handleRead

~~~C++
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = ::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG<< "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

~~~



​	•	**定义与功能**：

​	•	handleRead() 函数用于处理 wakeupFd_ 的可读事件。

​	•	它的作用是读取 wakeupFd_ 中的数据，并清除其内部计数器，以便重新进入等待状态。

​	•	**详细流程**：

​	•	创建一个 uint64_t one = 1 的变量，并通过 ::read() 从 wakeupFd_ 读取数据。

​	•	读取的数据是 eventfd 的计数器值，这样可以将 eventfd 计数器重置，防止它持续处于可读状态。

​	•	如果读取的字节数与 sizeof one 不一致，则通过日志记录读取到的字节数。



#### wakeupFd_ && wakeupChannel_

​	•	wakeupFd_:

​	•	wakeupFd_ 是通过 createEventfd() 函数创建的 eventfd 文件描述符，用于在 EventLoop 的不同线程间或事件循环中传递唤醒信号。

​	•	它是 EventLoop 中的一个成员变量，用于在需要时唤醒当前的 EventLoop 线程（比如有新任务时）。

​	•	wakeupChannel_:

​	•	wakeupChannel_ 是一个 Channel 对象，它负责监听 wakeupFd_ 的可读事件。

​	•	当 wakeupFd_ 被写入（即计数器增加）时，wakeupChannel_ 会捕捉到这个事件并调用 handleRead() 进行处理。

​	•	wakeupChannel_ 绑定了 wakeupFd_ 和 EventLoop，确保当有新的事件需要处理时，EventLoop 能够及时被唤醒并处理事件。



​	•	wakeup() 向 wakeupFd_ 写入数据，使其变为可读状态。

​	•	poll() 函数因为 wakeupFd_ 的可读状态而返回，并通过 wakeupChannel_ 调用 handleRead() 来处理这个可读事件。

​	•	这种机制确保了 EventLoop 在有任务需要处理时能被唤醒，避免一直处于阻塞状态。



#### dePendingFunctors

~~~C++
void EventLoop::dePendingFunctors() {
  std::vector<Functor> functors;
  callingPendingFunctors = true;

  {
    MutexLockGuard lock(mutex_);
    functors.swap(pendingFunctors_);
  }
  for(size_t i = 0;i<functors.size();i++) {
    functors[i]();
  }
  callingPendingFunctors = false;
}
~~~

上面的函数不是简单的在临界区依次调用Functor,而是把回调列表swap到局部变量functors中，这样一方面减小了临界区的长度，另一方面也避免了死锁