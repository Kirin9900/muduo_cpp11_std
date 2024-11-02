## muduo Buffer类的设计与应用



### muduo的IO模型

单线程linux下的IO模型：

阻塞(blocking), 非阻塞(non-blocking)  IO复用(IO multiplexing)   信号驱动(signal-driven)   异步(asynchronous)

对于one loop per thread 的模型，多线程服务端编程就是需要设计一个高效且易于使用的eventloop，然后每个线程run一个eventloop就行了

eventloop是非阻塞网络编程的核心，non-blocking几乎总是和IO multiplexing使用



### non-blocking =》应用层buffer

non-blocking IO的核心思想是避免阻塞在read()或write()或其他IO系统调用上，这样可以最大限度的复用thread-of-control，让每一个线程能服务于多个socket连接，IO线程只能阻塞在IO复用函数上。select/poll/epoll_wait，这样的话，应用层的缓冲是必需的，每个TCP socket都要有input buffer和output buffer



**TcpConnection必须要有output buffer**，如果需要发送100kb数据，操作系统只接受了80kb，程序要调用TcpConnection::send()发送数据，网络库要接管这些剩余的20kb，把它保存在该TCP connection的output buffer里，然后注册POLLOUT事件，一旦socket变得可写就立即发送数据。如果还有剩余，继续注册，不断关注POLLOUT

如果程序有写入了50kb,那么网络库不应该直接调用write,而是把这些数据append在那20KB的数据之后，等到socket可写再一并写入

在程序关闭连接时，必须确保outbuffer里面的数据发送完毕

所以要让程序在write操作上不阻塞，网络库必须要有output buffer



**TcpConnection必须要有input buffer**,网络库在处理socket可读事件的时候，必须一次性把socket的数据读完(从操作系统buffer搬运到应用程序buffer),否则会触发POLLIN事件，那么网络库必须要应对数据不完整的情况，收到的数据先放到input buffer里，等构成一条完整的消息在通知程序



所有muduo中的IO都是带缓冲的IO(buffered IO)，你不会自己去read()或write()某个socket,只会操作TcpConnection的input buffer和output buffer，更具体的说在onMessage()回调里面读取input buffer，调用TcpConnection::send()来间接操作output buffer，一般不会直接操作output buffer



### Buffer的功能需求

设计要点：

对外表现出一块连续的内存(char* p,int len)

其size()可以自动增长，以适应不同大小的消息

内部以std::vector<char>的形式来保存数据，并提供相应的访问函数

Buffer其实像是一个queue，从末尾写入数据，从头部读取数据



Input buffer .TcpConnection会从socket读取数据，然后写入input buffer(Buffer::readFd()完成的)，客户代码从input buffer读取数据。

output buffer 客户代码会把数据写入output buffer(TcpConnection::send()完成的)，TcpConnection从output buffer读取数据并写入socket


![Buffer](/img/img_3.png)

Buffer：：readFd()

**在栈上准备一个65536字节的extrabuf，然后利用readv()读取数据，iovec有两块，一块指向muduo Buffer中的writable字节，另一块指向extrabuf，这样读入的数据不多，那么就全部读到Buffer中去了。如果长度超过Buffer的writable字节数，就会读到栈上的extrabuf里，然后程序再把extrabuf里的数据append到buffer中去**



### Buffer的数据结构

muduo buffer的数据结构：

![buffer_data structure](/img/img_4.png)
内部是一个std::vector<char> ,它是一块连续的内存。此外Buffer有两个data member，指向vector的元素，这两个index的类型是int,不是char*

两个index把vector的内容分为三块：prependable,readable,writeable，灰色部分是有效载荷(payload)

prependable = readIndex

Readable = writeIndex - readIndex

Writable = size() - writeIndex

Muduo Buffer里有两个常数KCheapPrepend和KInitialSize,定义了prependable和writable的初始大小，readable的初始大小是0

![buffer2](/img/img_5.png)

### Buffer的操作

#### **基本的read-write  cycle**

Buffer初始化之后，如果想Buffer写入了200字节，那么其布局如下图：

![buffer3](/img/img_6.png)

writeIndex向后移动了200字节，readIndex保持不变，readable和writeable的值也有变化



如果从Buffer read()读入了50字节，那么readIndex向后移动了50字节，writeIndex保持不变，readable和writeable的值也有变化

![buffer4](/img/img_7.png)

然后又写入了200字节，writeIndex向后移动了200字节，readIndex保持不变

![buffer5](/img/img_8.png)

接下来，一次性读入350字节，由于全部数据读完了，readIndex和writeIndex返回原位以备新一轮使用

![buffer6](/img/img_9.png)

#### **自动增长**

muduo Buffer不是固定长度的，它可以自动增长，这是使用vector的直接好处

![buffer7](/img/img_10.png)

如果一次性写入1000字节，那么Buffer就会自动增长，此时的writeIndex会达到1358，但是readIndex返回到了前面，以保持prependable等于kCheapPrependable.由于vector重新分配了内存，原来指向元素的指针全部失效，这就是readIndex和writeIndex使用整数下标的原因

![buffer8](/img/img_16.png)

然后读入350字节，readIndex前移
![buffer9](/img/img_11.png)

最后读完剩下的1000字节，readIndex和writeIndex返回kCheapPrependable

![buffer10](/img/img_12.png)

Buffer并不会因为自动增长就会进行销毁，避免了内存浪费，又避免了反复分配内存

#### 内部腾挪

有时候经过若干次读写,readIndex已到了比较靠后的位置，留下了巨大的prependable空间

![buffer11](/img/img_13.png)

在这种情况下如果要写入超过writable的字节数，muduo Buffer不会重新分配内存，而是先把已有的数据移到前面去，腾出writable的空间

![buffer12](/img/img_14.png)

#### 前方添加(prepend)

muduo Buffer提供了prependable空间，让程序能够在很低的代价在数据前面添加几个字节

![buffer13](/img/img_15.png)

通过预留kCheapPrependable空间，可以简化客户代码，以空间换时间



### Buffer的关键代码




###### 类的基本结构

```cpp
class Buffer : public muduo::copyable
{
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;
```

- `Buffer` 类继承自 `muduo::copyable`（可能是一个标记类，用于表示这个类是可复制的）。

- `kCheapPrepend` 和 `kInitialSize` 是两个静态常量，分别表示预留的前置空间大小和初始缓冲区大小。

    - `kCheapPrepend = 8`：表示在缓冲区前面预留了 8 字节，用于在缓冲区前面插入数据。

    - `kInitialSize = 1024`：表示初始缓冲区大小为 1024 字节。



###### 构造函数

```cpp
Buffer()
  : buffer_(kCheapPrepend + kInitialSize),
    readerIndex_(kCheapPrepend),
    writerIndex_(kCheapPrepend)
{
  assert(readableBytes() == 0);
  assert(writableBytes() == kInitialSize);
  assert(prependableBytes() == kCheapPrepend);
}
```

- 构造函数创建一个大小为 `kCheapPrepend + kInitialSize` 的 `buffer_`。
- `readerIndex_` 和 `writerIndex_` 都初始化为 `kCheapPrepend`，意味着初始时，前面的 `kCheapPrepend` 字节是预留的，不用于存储数据。
- 使用 `assert` 确保缓冲区初始化后是空的，且可写字节数为 `kInitialSize`。



###### 索引相关方法

```cpp
size_t readableBytes() const
{ return writerIndex_ - readerIndex_; }

size_t writableBytes() const
{ return buffer_.size() - writerIndex_; }

size_t prependableBytes() const
{ return readerIndex_; }
```

- **`readableBytes()`**: 返回当前可读字节数，即从 `readerIndex_` 到 `writerIndex_` 之间的数据。
- **`writableBytes()`**: 返回当前可写字节数，即从 `writerIndex_` 到 `buffer_` 的末尾之间的空间。
- **`prependableBytes()`**: 返回前置的字节数，即从缓冲区开始到 `readerIndex_` 之间的空间。



###### 数据读取和操作

```cpp
const char* peek() const
{ return begin() + readerIndex_; }
```

- `peek()` 返回当前可读数据的起始指针，即从 `readerIndex_` 开始的位置。

```cpp
void retrieve(size_t len)
{
  assert(len <= readableBytes());
  readerIndex_ += len;
}
```

- `retrieve()` 用于从缓冲区中读取指定长度的数据，并移动 `readerIndex_` 来跳过这些数据。

```cpp
void retrieveAll()
{
  readerIndex_ = kCheapPrepend;
  writerIndex_ = kCheapPrepend;
}
```

- `retrieveAll()` 用于清空缓冲区，将 `readerIndex_` 和 `writerIndex_` 重置到初始位置。

```cpp
std::string retrieveAsString()
{
  std::string str(peek(), readableBytes());
  retrieveAll();
  return str;
}
```

- `retrieveAsString()` 读取所有可读数据，并将它们存储到一个 `std::string` 中，同时重置缓冲区。



###### 数据写入

```cpp
void append(const std::string& str)
{
  append(str.data(), str.length());
}
```

- `append()` 用于将 `std::string` 中的数据追加到缓冲区中。

```cpp
void append(const char* data, size_t len)
{
  ensureWritableBytes(len);
  std::copy(data, data + len, beginWrite());
  hasWritten(len);
}
```

- `append()` 函数用于将指定长度的数据追加到缓冲区。
- `ensureWritableBytes(len)` 确保有足够的可写空间。
- 使用 `std::copy` 将数据拷贝到 `beginWrite()` 所指向的位置，并更新写入位置。



###### 预留和缩减空间

```cpp
void ensureWritableBytes(size_t len)
{
  if (writableBytes() < len)
  {
    makeSpace(len);
  }
  assert(writableBytes() >= len);
}
```

- `ensureWritableBytes()` 确保缓冲区中有足够的空间存储新的数据，如果空间不足，调用 `makeSpace()` 扩展。

```cpp
void makeSpace(size_t len)
{
  if (writableBytes() + prependableBytes() < len + kCheapPrepend)
  {
    buffer_.resize(writerIndex_ + len);
  }
  else
  {
    size_t readable = readableBytes();
    std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
    readerIndex_ = kCheapPrepend;
    writerIndex_ = readerIndex_ + readable;
  }
}
```

- `makeSpace()` 在可写空间不足时调整缓冲区的大小。
    - 如果整个缓冲区（包括前置空间）的大小不足以存储新的数据，就将 `buffer_` 扩展到足够大。
    - 否则，将现有的可读数据移到缓冲区的前部，从而腾出更多空间。



###### readFd 函数

```cpp
ssize_t readFd(int fd, int* savedErrno);
```

- `readFd` 是一个成员函数，用于从文件描述符 `fd` 中读取数据，详情已经在前面分析过。



##### 其他函数

- **`swap()`**: 用于与另一个 `Buffer` 对象交换数据，常用于高效地交换内容。
- **`shrink()`**: 用于减少缓冲区的大小，保留一定的预留空间以避免浪费。



#### Buffer.cc

这段代码实现了 `Buffer` 类中的 `readFd` 方法，它用于从文件描述符 `fd` 中读取数据到缓冲区。`readFd` 方法利用了 `readv` 系统调用，该调用可以通过一个 `iovec` 数组将数据读取到多个缓冲区。

##### 代码解析：

```cpp
ssize_t Buffer::readFd(int fd, int* savedErrno)
```

- `readFd` 是 `Buffer` 类的一个成员函数，用于从文件描述符 `fd` 读取数据。
- `fd` 是文件描述符，可以是套接字描述符，用于网络通信。
- `savedErrno` 是一个指针，用于在读取失败时存储 `errno`（即系统调用错误代码）。

```cpp
char extrabuf[65536];
struct iovec vec[2];
const size_t writable = writableBytes();
```

- `extrabuf` 是一个临时缓冲区，大小为 65536 字节（64KB），用于辅助读取数据。
- `vec` 是 `iovec` 类型的数组，包含两个元素，用于 `readv` 系统调用。
- `writable` 记录当前 `Buffer` 对象内部缓冲区中可写入的字节数。

```cpp
vec[0].iov_base = begin() + writerIndex_;
vec[0].iov_len = writable;
vec[1].iov_base = extrabuf;
vec[1].iov_len = sizeof extrabuf;
```

- `vec[0]` 表示 `Buffer` 对象内部缓冲区的可写部分。
    - `iov_base` 是指向数据的指针，设置为 `Buffer` 可写区域的起始位置。
    - `iov_len` 是该区域的长度，设置为 `writable`。
- `vec[1]` 表示辅助缓冲区 `extrabuf`，当 `Buffer` 内部的可写空间不足时，额外的数据将被读入 `extrabuf`。

```cpp
const ssize_t n = readv(fd, vec, 2);
```

- 调用了 `readv` 系统调用，从文件描述符 `fd` 读取数据，并将数据填充到 `vec` 指定的两个缓冲区中。
    - `vec` 包含 `Buffer` 对象的可写区域和 `extrabuf`。
    - `n` 表示读取的总字节数。

```cpp
if (n < 0) {
  *savedErrno = errno;
}
```

- 如果 `n` 小于 0，表示读取失败，将 `errno` 保存到 `savedErrno` 中。

```cpp
else if (implicit_cast<size_t>(n) <= writable) {
  writerIndex_ += n;
}
```

- 如果读取的字节数 `n` 小于或等于 `Buffer` 内部缓冲区的可写部分长度 `writable`，说明所有数据都读入到了 `Buffer` 的内部缓冲区中。
- 此时，只需增加 `writerIndex_`（写入指针）以反映缓冲区中新数据的位置。

```cpp
else {
  writerIndex_ = buffer_.size();
  append(extrabuf, n - writable);
}
```

- 如果读取的字节数 `n` 超过了 `Buffer` 内部的可写空间 `writable`，说明部分数据被读入到了 `extrabuf` 中。
- 此时，将 `writerIndex_` 移到 `Buffer` 内部缓冲区的末尾。
- 然后调用 `append` 方法，将 `extrabuf` 中的剩余数据（`n - writable` 字节）添加到 `Buffer`。

```cpp
return n;
```

- 返回读取的总字节数 `n`。

###### 总结：

- `readFd` 方法通过 `readv` 调用从文件描述符读取数据，并尝试首先将数据写入 `Buffer` 内部的缓冲区。
- 如果内部缓冲区不够写，则使用一个额外的 `extrabuf` 来暂时存储多余的数据，再将其追加到 `Buffer` 中。
- 这种方式可以减少多次读取的系统调用次数，从而提高读取数据的效率。



#### 扩展：iovec数据类型的数组

`iovec` 类型的数组是一个用于向 `readv` 和 `writev` 系统调用提供多个缓冲区的结构体数组。`iovec` 是在 `<sys/uio.h>` 头文件中定义的结构体，用于描述一块内存缓冲区。

##### `iovec` 结构体定义：

```cpp
struct iovec {
    void  *iov_base;  // 指向缓冲区的指针
    size_t iov_len;   // 缓冲区的长度
};
```

- **`iov_base`**: 指向实际数据存储的缓冲区地址，即数据的起始位置。
- **`iov_len`**: 缓冲区的长度，表示可以读取或写入的字节数。

##### 使用场景：

`iovec` 结构体的数组通常用于 `readv` 和 `writev` 系统调用：

- **`readv`**: 从文件描述符读取数据，并将数据依次填充到 `iovec` 数组中的各个缓冲区中。
- **`writev`**: 将 `iovec` 数组中的多个缓冲区的数据依次写入到文件描述符中。

##### 示例：

```cpp
char buffer1[1024];
char buffer2[2048];
struct iovec vec[2];
vec[0].iov_base = buffer1;
vec[0].iov_len = sizeof(buffer1);
vec[1].iov_base = buffer2;
vec[1].iov_len = sizeof(buffer2);
```

在上面的例子中：

- `vec` 是一个 `iovec` 类型的数组，包含了两个元素。
- `vec[0]` 和 `vec[1]` 分别指向两个不同的缓冲区 `buffer1` 和 `buffer2`。
- `iov_base` 和 `iov_len` 分别设置为每个缓冲区的地址和大小。

##### `readv` 和 `writev` 系统调用：

- **`readv(int fd, const struct iovec *iov, int iovcnt)`**:
    - 从文件描述符 `fd` 中读取数据，将数据依次填充到 `iov` 数组的每个缓冲区中。
    - `iovcnt` 是 `iov` 数组的元素个数，即缓冲区的数量。
    - 返回值是读取的总字节数。

- **`writev(int fd, const struct iovec *iov, int iovcnt)`**:
    - 将 `iov` 数组中的每个缓冲区的数据依次写入到文件描述符 `fd` 中。
    - `iovcnt` 是 `iov` 数组的元素个数。
    - 返回值是写入的总字节数。

##### 举例：

如果我们有一个 `iovec` 数组 `vec`，其中包含两个缓冲区：

```cpp
struct iovec vec[2];
vec[0].iov_base = "Hello, ";
vec[0].iov_len = 7;
vec[1].iov_base = "world!";
vec[1].iov_len = 6;
```

调用 `writev(fd, vec, 2)` 时，系统会依次将 `"Hello, "` 和 `"world!"` 写入文件描述符 `fd` 中，效果相当于一次性写入 `"Hello, world!"`。

##### 总结：

`iovec` 类型的数组允许程序使用单次系统调用来处理多个内存缓冲区的数据传输，这样可以提高 I/O 的效率，减少系统调用次数。它在需要从多个缓冲区读取或写入数据时非常有用。



