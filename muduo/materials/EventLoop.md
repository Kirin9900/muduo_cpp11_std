## EventLoop 

**基本接口**：

构造函数  析构函数   loop()成员函数

注意eventLoop是不可拷贝的，muduo中大多数class都是不可拷贝的

~~~C++
class EventLoop {
public:
  EventLoop();
  ~EventLoop();
  void loop();
  void assertInLoopThread() {
    if(!isInLoopThread()) {
      abortNotInLoopThread();
    }
  }

  bool isInLoopThread() const{return threadId_ == CurrentThread::tid();}

private:
  void abortNotInLoopThread();
  bool looping_;  //atomic
  const pid_t threadId_;
};

~~~



one loop per thread 每个线程都只能有一个EventLoop对象，因此EventLoop的构造函数会检查当前线程是否创建了其他EventLoop对象，遇到错误就终止程序

EventLoop的构造函数会记住本对象所属的线程(threadId_).**创建了EventLoop对象的线程是IO线程**，其主要功能是运行事件循环EventLoop::loop() . EventLoop对象的生命周期通常和其所属的线程一样长。



~~~C++
__thread EventLoop* t_loopInThisThread = 0;

EventLoop::EventLoop():looping_(false),threadId_(CurrentThread::tid()) {
  
  LOG<<"Eventloop created"<<this<<"in thread"<<threadId_;
  if(t_loopInThisThread) {
    LOG<<"Another EventLoop"<<t_loopInThisThread
    <<"exists in this thread"<<threadId_;
  }else {
    t_loopInThisThread = this;
  }
}

EventLoop::~EventLoop() {
  assert(!looping_);
  t_loopInThisThread = nullptr;
}

~~~



### 事件循环

**事件循环必须在IO线程执行**，因此EventLoop::loop()会检查某些准备条件：哪些成员函数是线程安全的，哪些可以跨线程调用，哪些成员函数在某个特定线程调用(主要是IO线程)

~~~C++
//temp do nothing  wait for 5 seconds
void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;

  ::poll(nullptr,0,5*1000);
  LOG<< "EventLoop "<<this<<" stop looping";

  looping_ = false;

}
~~~



