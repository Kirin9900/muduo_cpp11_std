# Epoll

epoll是linux独有的高效的IO multiplexing机制，它与poll的不同之处在于poll每次返回整个文件描述符数组，用户需要便利数组以找到哪些文件描述符上的IO事件，而epoll_wait返回的是活动fd的列表，需要遍历的数组通常会小得多。在并发连接数较大而活动连接比例不高时，epoll比poll更高效

muduo将epoll封装为EPoll class 与Poller class 具有完全相同的接口


~~~ C++
using namespace muduo;

static_assert(EPOLLIN == POLLIN, "EPOLLIN and POLLIN constants must match");
static_assert(EPOLLPRI == POLLPRI, "EPOLLPRI and POLLPRI constants must match");
static_assert(EPOLLOUT == POLLOUT, "EPOLLOUT and POLLOUT constants must match");
static_assert(EPOLLRDHUP == POLLRDHUP, "EPOLLRDHUP and POLLRDHUP constants must match");
static_assert(EPOLLERR == POLLERR, "EPOLLERR and POLLERR constants must match");
static_assert(EPOLLHUP == POLLHUP, "EPOLLHUP and POLLHUP constants must match");


const int kNew = -1;
const int kAdded = 1;
const int kDeleted  = 2;

//构造函数，创建epoll实例，并分配初始事件列表大小
 EPoller::EPoller(EventLoop* loop)
   :ownerLoop_(loop),
    epollfd_(::epoll_create(EPOLL_CLOEXEC)),
    events_(kInitEventListSize)
{
   if(epollfd_ < 0) {
     LOG << "EPoller::EPoller()";
   }
}

//析构函数：关闭epoll文件描述符
 EPoller::~EPoller()
{
   ::close(epollfd_);
}

//轮询IO事件，并将活跃的事件填充到activeChannels列表中

Timestamp EPoller::poll(int timeoutMs,ChannelList* activeChannels)
{
   int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
   Timestamp now = Timestamp::now();
   if(numEvents < 0) {
     LOG<<numEvents<<"events happened";
     fillActiveChannels(numEvents,activeChannels);
     if(static_cast<size_t>(numEvents) == events_.size()) {
       events_.resize(events_.size() * 2);
     }
   }else if(numEvents == 0) {
     LOG<<" nothing happened";
   }else {
     LOG<<"EPoller::poll()";
   }
   return now;
}

//将活跃事件填充到activeChannels列表中
void EPoller::fillActiveChannels(int numEvents,ChannelList* activeChannels) const
{
   assert(static_cast<size_t>(numEvents)<events_.size());
   for(int i=0;i<numEvents;++i) {
     Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
     channel -> set_revents(events_[i].events); //设置事件
     activeChannels->push_back(channel);
   }
 }

//更新epoll中的Channel，根据需要添加，修改或删除十斤啊

void EPoller::updateChannel(Channel* channel)
{
   assertInLoopThread();
   const int index = channel->index();
   if(index == kNew || index == kDeleted) {
     int fd = channel->fd();
     if(index  == kNew) {
       channels_[fd] = channel;
     }
     channel->set_index(kAdded);
     update(EPOLL_CTL_ADD,channel);
   }else {
     int fd  = channel->fd();
     if(channel->isNoneEvent()) {
       update(EPOLL_CTL_DEL, channel);
       channel->set_index(kDeleted);
     }else {
       update(EPOLL_CTL_MOD,channel);
     }
   }
}

//移除epoll中的Channel
void EPoller::removeChannel(Channel* channel){
   assertInLoopThread();
   int fd = channel->fd();
   channels_.erase(fd);

   if(channel->index() == kAdded) {
     update(EPOLL_CTL_DEL,channel);
   }
   channel->set_index(kNew);
}

//执行epoll的事件更新操作(添加，修改或删除)
void EPoller::update(int operation,Channel* channel) {
   struct epoll_event event;
   bzero(&event,sizeof event);
   event.events = channel->events();
   event.data.ptr = channel;
   int fd  = channel->fd();
   if(::epoll_ctl(epollfd_,operation,fd,&event)<0) {
     if(operation == EPOLL_CTL_DEL) {
       if(operation == EPOLL_CTL_DEL) {
         LOG<<"epoll_ctl op=" << operation << " fd=" << fd;
       }else {
         LOG<<"epoll_ctl op=" << operation << " fd=" << fd;
       }
     }
   }
 }
~~~


`EPoller` 类是 Muduo 网络库中的一个事件多路复用类，它使用 `epoll` 作为 I/O 事件的监控机制。该类负责在 I/O 多路复用中注册、更新、删除文件描述符的事件，并在 `EventLoop` 主循环中轮询事件。以下是对其成员变量和成员函数的详细解析：

### 成员变量解析

```cpp
EventLoop* ownerLoop_;
int epollfd_;
std::vector<struct epoll_event> events_;
std::unordered_map<int, Channel*> channels_;
```

1. **`EventLoop* ownerLoop_;`**
    - 事件循环对象指针，用于关联该 `EPoller` 所属的 `EventLoop`，确保线程安全。

2. **`int epollfd_;`**
    - `epoll` 文件描述符，用于管理 `epoll` 实例，`epoll_create` 返回的描述符会被存储在此变量中。

3. **`std::vector<struct epoll_event> events_;`**
    - `epoll` 事件列表，存储已注册的文件描述符和对应事件类型，每次调用 `epoll_wait` 后填充有活跃事件。

4. **`std::unordered_map<int, Channel*> channels_;`**
    - 存储所有已注册的 `Channel` 对象，键为文件描述符 `fd`，值为对应的 `Channel*`。

### 成员函数解析

#### `EPoller::EPoller(EventLoop* loop)`

构造函数，创建 `epoll` 实例并初始化事件列表：

- **`ownerLoop_(loop)`**：将 `EPoller` 对象与指定的 `EventLoop` 关联。
- **`epollfd_(::epoll_create(EPOLL_CLOEXEC))`**：创建 `epoll` 实例，`EPOLL_CLOEXEC` 表示子进程不会继承此文件描述符。
- **`events_(kInitEventListSize)`**：分配初始事件列表大小。

#### `EPoller::~EPoller()`

析构函数，关闭 `epoll` 文件描述符，释放资源：

- **`::close(epollfd_);`**：关闭 `epoll` 文件描述符。

#### `Timestamp EPoller::poll(int timeoutMs, ChannelList* activeChannels)`

轮询 `epoll` 实例以检测 I/O 事件，并将活跃事件填充到 `activeChannels` 列表：

- **`::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);`**
    - 调用 `epoll_wait`，等待 I/O 事件发生，超时由 `timeoutMs` 控制。
    - 返回值 `numEvents` 为发生的事件数。

- **`fillActiveChannels(numEvents, activeChannels);`**：将已就绪的事件填充到 `activeChannels` 列表。
- **`events_.resize(events_.size() * 2);`**：若事件数达到当前容量，扩展事件列表大小。

#### `void EPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const`

遍历发生的事件，并将每个事件对应的 `Channel` 对象加入 `activeChannels`：

- **`channel->set_revents(events_[i].events);`**：设置事件类型。
- **`activeChannels->push_back(channel);`**：将对应的 `Channel` 添加到活跃事件列表中。

#### `void EPoller::updateChannel(Channel* channel)`

在 `epoll` 实例中更新 `Channel`，根据 `Channel` 的状态选择是添加、修改还是删除该事件：

- **`int index = channel->index();`**：获取 `Channel` 当前的状态。
- **`update(EPOLL_CTL_ADD, channel);`**：若为新 `Channel`，在 `epoll` 中注册该事件。
- **`update(EPOLL_CTL_DEL, channel);`**：若无事件则从 `epoll` 中删除。
- **`update(EPOLL_CTL_MOD, channel);`**：若已存在且有事件变化，则修改其监听事件。

#### `void EPoller::removeChannel(Channel* channel)`

从 `epoll` 实例中移除指定 `Channel`：

- **`channels_.erase(fd);`**：从 `channels_` 容器中删除该 `Channel`。
- **`update(EPOLL_CTL_DEL, channel);`**：从 `epoll` 中删除对应的事件。

#### `void EPoller::update(int operation, Channel* channel)`

执行 `epoll_ctl` 操作，添加、修改或删除事件：

- **`bzero(&event, sizeof event);`**：初始化事件结构体。
- **`event.events = channel->events();`**：设置事件类型。
- **`event.data.ptr = channel;`**：将 `Channel` 的指针存入事件数据。
- **`::epoll_ctl(epollfd_, operation, fd, &event);`**：调用 `epoll_ctl`，执行指定的 `operation` 操作。

### 整体作用

`EPoller` 类是 `EventLoop` 的 I/O 多路复用机制的核心实现，通过封装 `epoll` API，实现了事件的注册、更新、删除以及活跃事件的轮询。它将 `Channel` 对象与 `epoll` 事件管理关联，实现了高效的 I/O 事件监控和分发机制。


