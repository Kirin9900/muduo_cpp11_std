# Acceptor

先定义Accerptor class,用于accept新TCP连接，并通过回调通知使用者。它是内部class，供TcpServer使用，生命期一片后者管理

~~~C++
class Acceptor {
public:
  typedef std::function<void (int sockfd,
                              const InetAddress&)> NewConnectionCallback;
  Acceptor(EventLoop* loop,const InetAddress& listenAddr);
  void setnewConnectionCallback(const NewConnectionCallback& cb) {
    
  }
  bool listening()const{}
  void listen();
};
~~~

Acceptor的数据成员包括Socket,Channel等。其中Socket是一个RALL handle，封装了socket文件描述符的生命期。Acceptor的socket是listening socket,即server socket.Channel用于观察此socket上的readable事件，并调用Acceptor::handleRead(),后者会调用accept来接受新的连接，并回调用户callback

~~~C++
private:
  void handleRead();

  EventLoop* loop_;
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listening_;
~~~

Acceptor的构造函数和Acceptor::listen()成员函数执行创建TCP服务端的传统步，调用socket,bind,listen等Sockets API

~~~C++
Acceptor::Acceptor(EventLoop* loop,const InetAddress& listenAddr)
  :loop_(loop),
   acceptSocket_(sockets::createNonblockingOrDie()),
   acceptChannel_(loop,acceptSocket_.fd()),
   listening_(false)
{
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.bindAddress(listenAddr);
  acceptChannel_.setWReadCallback(std::bind(&Acceptor::handleRead,this));
}
void Acceptor::listen() {
  loop_->assertInLoopThread();
  listening_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading();
}

~~~

Acceptor的接口中用到了InetAddress class ,这是对struct sockaddr_in的简单封装，能自动转换字节序

Acceptor的构造函数用到createNonblockingOrDie()来创建非阻塞的socket.

Acceptor:listen()的最后一步让acceptChannel_在socket可读的时候调用Acceptor::handleRead(),后者会接受(accept)并回调newConnectionCall-back_


### 关键代码



`Acceptor` 类主要用于处理服务端监听 socket 的接受连接请求操作。在 Muduo 网络库中，`Acceptor` 负责创建一个监听 socket 并管理其事件，并在有新连接到达时触发回调函数将其处理。它依赖 `EventLoop` 来管理 I/O 事件，并使用回调函数机制在新连接建立时通知上层逻辑。

### 主要成员变量

```cpp
EventLoop* loop_;
Socket acceptSocket_;
Channel acceptChannel_;
bool listening_;
std::function<void(int sockfd, const InetAddress&)> newConnectionCallback_;
```

1. **`EventLoop* loop_;`**
    - 指向 `EventLoop` 对象的指针，用于管理 `Acceptor` 的事件循环。

2. **`Socket acceptSocket_;`**
    - 管理监听 socket 的 `Socket` 对象。通过它进行 socket 创建、地址绑定、监听等操作。

3. **`Channel acceptChannel_;`**
    - 事件管理器 `Channel` 对象，将 `acceptSocket_` 与事件循环 `loop_` 绑定。负责将 `acceptSocket_` 上的事件分发给 `EventLoop`，并触发相应的回调函数。

4. **`bool listening_;`**
    - 指示是否正在监听。为 `true` 表示正在监听，为 `false` 表示还未开始监听。

5. **`std::function<void(int sockfd, const InetAddress&)> newConnectionCallback_;`**
    - 新连接到达时的回调函数。接受两个参数：新连接的文件描述符 `sockfd` 和客户端的地址 `peerAddr`。

### 主要成员函数

#### `Acceptor(EventLoop* loop, const InetAddress& listenAddr)`

构造函数，用于初始化 `Acceptor`，并设置 `acceptSocket_` 以便于绑定、监听和处理新连接：

- **`loop_`**：将 `loop_` 设置为指定的 `EventLoop`。
- **`acceptSocket_`**：创建非阻塞的监听 socket。
- **`acceptChannel_`**：将 `acceptSocket_` 绑定到事件循环 `loop_`。
- **`acceptChannel_.setWReadCallback(std::bind(&Acceptor::handleRead, this));`**：设置读事件回调，当有新连接到达时将调用 `handleRead()`。

#### `void listen()`

启动监听操作：

- **`loop_->assertInLoopThread()`**：确保该操作在 `loop_` 所属的线程中执行。
- **`listening_ = true;`**：设置 `listening_` 为 `true`，表示开始监听。
- **`acceptSocket_.listen();`**：调用 `Socket` 的 `listen()` 方法，实际执行监听。
- **`acceptChannel_.enableReading();`**：启用读事件监听，以便 `EventLoop` 可以捕获到新的连接请求并调用回调函数。

#### `void handleRead()`

处理新连接的回调函数，当 `acceptChannel_` 上有事件时调用：

1. **`loop_->assertInLoopThread();`**：确保此操作在 `loop_` 所属的线程中执行。
2. **`InetAddress peerAddr(0);`**：创建一个 `InetAddress` 对象，用于存储新连接的客户端地址信息。
3. **`int connfd = acceptSocket_.accept(&peerAddr);`**：调用 `accept()`，接收新连接并获取连接的文件描述符 `connfd` 和客户端地址 `peerAddr`。
4. **条件判断**：
    - **`if (connfd >= 0)`**：如果连接成功（`connfd` 有效），检查是否有新连接回调函数。
        - **`if (newConnectionCallback_)`**：如果设置了回调函数 `newConnectionCallback_`，则调用它来处理新连接。
        - **`else`**：如果未设置回调函数，则关闭连接。
    - `sockets::close(connfd);`：避免未处理的连接导致资源泄漏。

### 整体作用

`Acceptor` 主要负责创建监听 socket 并在有新连接时触发回调通知。通过 `Acceptor`，服务端可以将新连接的处理与其他 I/O 操作隔离开来，使得网络库结构更加清晰，且便于将新连接的逻辑分离到更高级的管理层中。