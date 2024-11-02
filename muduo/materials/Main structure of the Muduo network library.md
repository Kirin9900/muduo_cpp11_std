## muduo网络库主要结构

muduo是基于Reactor模式的网络库，其核心是整个事件循环EventLoop,用于响应计时器和IO事件。采用基于对象而非面向对象的设计风格，事件回调接口多采用boost:function+boost::bind表达



### 公开接口

Buffer 缓冲区  数据的读写通过buffer进行，用户代码不需要调用read和write，只需要处理收到的数据和准备好要发送的数据即可

InetAddress封装IPv4地址(end point)  它**不能解析域名，只负责IP地址**

EventLoop 事件循环(反应器Reactor)，**每个线程只能有一个EventLoop实体**，它**负责IO和定时器事件的分派，它用eventfd来异步唤醒，使用TimerQueue作为计时器管理，用poller作为IO multiplexing(IO多路分发器)**

EventLoopThread 启动一个线程，在其中运行EventLoop:loop()

TcpConnection为整个网络库的核心，封装一次TCP连接，注意它**不能发起连接**

TcpClient 用于编写网络客户端，能**发起**连接，并且有重试功能

TcpServer 用于编写网络服务器，**接受**客户的连接



TcpConnection的生命周期依靠s**hared_ptr管理(用户和库共同管理**).Buffer的生命周期由**TcpConnection控制**，其余类的生命期由用户控制。**Buffer和InetAddress 可以拷贝**



### 内部实现

Channel是selectable IO channel，负责**注册与响应 **IO事件**，注意它不拥有fd,它Acceptor,Connector,Eventloop,TimerQueue,TcpConnection的成员，生命期由后者控制

Socket是一个RALL handle，封装一个fd，并在析构时关闭fd,它是Acceptor,TcpConnection的成员，生命周期由后者控制。

Poller 采用level trigger,它是Eventloop的成员

**Connector用于发起TCP连接**，它是TcpClient的成员，生命期由后者管理

**Acceptor用于接受TCP连接**，它是TcpServer的成员，生命期由后者管理

TimerQueue用timefd实现定时，**用map管理Timer，常用复杂度O(log N)，N为定时器成员**，属于EventLoop

EventLoopThreadPool用于创建IO线程池，**用于把TcpConnection分派到某个EventLoop线程上。它是TcpServer的成员，生命周期由后者管理**

![muduo_structure](/img/img_2.png)





### 线程模型

one loop per thread +thread pool模型

每个线程最多有一个EventLoop,每个TcpConnection必须归某个EventLoop管理，所有的IO都会转移到这个线程，一个fd只能由一个线程读写

TcpServer支持多线程，accept与EventLoop在同一个线程，另外创建一个EventLoop-ThreadPool，新到的连接会按照round-robin方式分配到线程池