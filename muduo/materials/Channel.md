# Channel  class

Channe class，每个Channel对象自始至终都只属于一个EventLoop。因此每个Channel对象都只属于一个IO线程。每个Channel对象自始至终只负责一个文件描述符的事件分发，但它并不拥有这个fd,也不会在析构的时候关闭这个fd.

Channel会把不同的IO事件分发为不同的回调，例如readCallback,WriteCallback等，用户无须继承这个Channel，而会使用更上层的封装，如**TcpConnection**.Channel的生命期由其owner class 负责管理

~~~C++
#ifndef CHANNEL_H
#define CHANNEL_H
#include <functional>
namespace muduo {
class EventLoop;

class Channel {
public:
  typedef std::function<void ()> EventCallback;
  Channel(EventLoop* loop,int fd);
  void handleEvent();
  void setWriteCallback(const EventCallback& cb) {
    writeCallback_ = cb;
  }
  void setWReadCallback(const EventCallback& cb) {
    readCallback_ = cb;
  }
  void setErrorCallback(const EventCallback& cb) {
    errorCallback_ = cb;
  }
  int fd() const{return fd_;}
  int events() const{return events_;}
  void set_revents(int revt){revents_ = revt;}
  bool isNoneEvent()const{return events_ == kNoneEvent;}
  void enableReading(){events_|=kReadEvent;update();}
  void enableWriting(){events_|=kWriteEvent;update();}
  void disableWriting(){events_&= ~kWriteEvent;update();}
  void disableAll(){events_= kNoneEvent;update();}



  //for Poller
  int index(){return index_;}
  void set_index(int idx){index_ = idx;}

  EventLoop* ownerLoop() {
    return loop_;
  }
private:
  void update();

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;
  const int fd_;
  int events_;
  int revents_;
  int index_;

  EventCallback writeCallback_;
  EventCallback errorCallback_;
  EventCallback readCallback_;

};
}
#endif //CHANNEL_H

~~~

有些成员函数是内部使用的，用户一般只使用set*Callback()和enableReading这几个函数。Channel的成员函数都只能在IO线程调用，因此更新数据成员都不必加锁

events_是它关心的IO事件，由用户设置；revents_是目前活动的事件，由EventLoop/Poller设置，这两个字段都是bit pattern,它们的名字来自poll的struct pollfd; fd是监听事件的文件描述符

Channel::update()会调用EventLoop::updateChannel()，后者会转而调用Poller::updateChannel()



~~~~C++
#include "Channel.h"

#include <bits/poll.h>

const int muduo::Channel::kNoneEvent = 0;
const int muduo::Channel::kReadEvent = POLLIN/POLLPRI;
const int muduo::Channel::kWriteEvent = POLLOUT;

muduo::Channel::Channel(muduo::EventLoop* loop,int fdArg)
  :loop_(loop),fd_(fdArg),events_(0),revents_(0),index_(-1)
{

}

void muduo::Channel::update() {
  //loop_->updateChannel(this);
}
~~~~



Channel::handleEvent()是Channel的核心，它由EventLoop:loop()调用,它的功能是根据revents_的值分别调用不同的回调

`POLLERR`、`POLLNVAL`、`POLLIN`、`POLLPRI` 和 `POLLOUT` 是在使用 `poll` 系统调用进行 I/O 多路复用时用到的事件标志，它们用于表示文件描述符（如套接字）上可能发生的不同类型的 I/O 事件。每个标志都代表一种特定类型的状态或事件，以下是它们的具体含义：

| 标志       | 含义           | 典型用途                       |
| ---------- | -------------- | ------------------------------ |
| `POLLERR`  | 发生错误       | 错误处理，检查套接字或设备状态 |
| `POLLNVAL` | 文件描述符无效 | 检查传入的文件描述符是否正确   |
| `POLLIN`   | 有数据可读     | 从套接字读取数据或接收消息     |
| `POLLPRI`  | 有紧急数据可读 | 处理带外数据或紧急网络控制信息 |
| `POLLOUT`  | 可写           | 向套接字发送数据               |




### 关键代码



该 `Channel` 类是 Muduo 网络库中用于事件管理的一个核心组件。`Channel` 类代表一个文件描述符（例如 socket）上的一组 I/O 事件，并为这些事件的处理提供回调函数接口。`Channel` 并不负责事件的分发，而是将事件分发的工作交给其所属的 `EventLoop`。

### 主要成员变量

```cpp
muduo::EventLoop* loop_;
int fd_;
int events_;
int revents_;
int index_;
bool eventHandling_;
std::function<void()> errorCallback_;
std::function<void()> closeCallback_;
std::function<void(Timestamp)> readCallback_;
std::function<void()> writeCallback_;
```

1. **`muduo::EventLoop* loop_;`**
    - 指向所属的 `EventLoop` 对象。`Channel` 的事件更新、删除等操作通过 `loop_` 来实现。

2. **`int fd_;`**
    - 文件描述符，即所监听的 I/O 对象。通常是一个 socket 文件描述符，`Channel` 将在此文件描述符上检测事件。

3. **`int events_;`**
    - 表示感兴趣的事件。`Channel` 注册到 `poll` 或 `epoll` 时，`events_` 指定了哪些事件需要监听。

4. **`int revents_;`**
    - 表示实际发生的事件。每次事件循环返回时，将会设置 `revents_`，以指示当前 `Channel` 上触发的事件类型。

5. **`int index_;`**
    - 用于标记 `Channel` 在 `poll` 或 `epoll` 中的状态，比如是否已经被添加到 `poll`/`epoll` 中。

6. **`bool eventHandling_;`**
    - 标记当前是否正在处理事件。用于确保在事件处理过程中不发生资源竞争或重复处理。

7. **`std::function<void()> errorCallback_;`**
    - 当 `Channel` 检测到错误事件（`POLLERR`、`POLLNVAL`）时执行的回调函数。

8. **`std::function<void()> closeCallback_;`**
    - 当连接关闭（`POLLHUP`）事件触发时调用的回调函数。

9. **`std::function<void(Timestamp)> readCallback_;`**
    - 当读事件（`POLLIN`、`POLLPRI`、`POLLRDHUP`）触发时调用的回调函数，参数 `receiveTime` 为事件触发的时间戳。

10. **`std::function<void()> writeCallback_;`**
    - 当写事件（`POLLOUT`）触发时调用的回调函数。

### 常量

```cpp
const int muduo::Channel::kNoneEvent = 0;
const int muduo::Channel::kReadEvent = POLLIN | POLLPRI;
const int muduo::Channel::kWriteEvent = POLLOUT;
```

- **`kNoneEvent`**：表示不关注任何事件。
- **`kReadEvent`**：表示关注读事件（数据可读或紧急数据可读）。
- **`kWriteEvent`**：表示关注写事件（数据可写）。

### 主要成员函数

#### `Channel(EventLoop* loop, int fdArg)`

构造函数，初始化 `Channel` 的成员变量：
- `loop_` 被设置为指定的 `EventLoop` 对象。
- `fd_` 被设置为指定的文件描述符。
- `events_` 被初始化为 `0`，表示暂不关注任何事件。
- `revents_` 被初始化为 `0`，表示没有实际事件发生。
- `index_` 被设置为 `-1`，表示未添加到 `poll`/`epoll`。
- `eventHandling_` 被设置为 `false`，表示当前没有正在处理事件。

#### `~Channel()`

析构函数，用于确保 `eventHandling_` 为 `false`，即在析构时不应有事件处理正在进行。

#### `void update()`

调用 `loop_->updateChannel(this);`，通知 `EventLoop` 更新当前 `Channel`。通常用于当 `events_` 发生改变时调用，以重新注册需要监听的事件。

#### `void handleEvent(Timestamp receiveTime)`

处理当前 `Channel` 上的事件，步骤如下：

1. 设置 `eventHandling_` 为 `true`，以标记正在处理事件。
2. 检查是否有无效事件 `POLLNVAL`，并在日志中记录。
3. 检查是否有错误事件 `POLLERR` 或 `POLLNVAL`，并调用 `errorCallback_`。
4. 检查是否有挂起事件 `POLLHUP` 且无读事件 `POLLIN`，并调用 `closeCallback_`。
5. 检查是否有读事件（`POLLIN`、`POLLPRI` 或 `POLLRDHUP`），并调用 `readCallback_`，传入事件发生的时间戳 `receiveTime`。
6. 检查是否有写事件 `POLLOUT`，并调用 `writeCallback_`。
7. 事件处理完成后，将 `eventHandling_` 设置回 `false`。

### 整体作用

`Channel` 是一个文件描述符的事件抽象层，它在 `EventLoop` 中充当桥梁。它不仅将 `poll` 或 `epoll` 的事件检测到的 I/O 事件转化为回调函数，还将事件注册、删除等操作传递到 `EventLoop` 中。