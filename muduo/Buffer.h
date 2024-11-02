

#ifndef BUFFER_H
#define BUFFER_H
#include <algorithm>
#include <string>
#include <vector>
#include <cassert>

#include <unistd.h>


namespace muduo {
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer {
public:
  static const size_t kCheapPrepend = 8; //初始化预留空间 prependable
  static const size_t kInitialSize = 1024;//writeable

  Buffer():
  buffer_(kCheapPrepend+kInitialSize),readIndex(kCheapPrepend),writeIndex(kInitialSize) {

  }

  void swap_demo(Buffer& rhs) {
    std::swap(readIndex,rhs.readIndex);
    std::swap(writeIndex,rhs.writeIndex);
  }

  size_t readableBytes() const {
    return writeIndex - readIndex;
  }

  size_t writableBytes() const {
    return buffer_.size() - writeIndex;
  }

  size_t prependableBytes() const {
    return readIndex;
  }
  /*
  •peek()：返回当前可读数据的起始位置（指针）。
  •retrieve()：移动 readIndex，丢弃一定长度的可读数据，表示这些数据已经被处理。
  •retrieveUntil()：将 readIndex 移动到指定的地址 end，丢弃从 readIndex 到 end 之间的数据。
   */

  const char* peek() const {
    return begin() + readIndex;  //begin() + readIndex
  }

  // retrieve returns void, to prevent
  // string str(retrieve(readableBytes()), readableBytes());
  // the evaluation of two functions are unspecified

  void retrieve(size_t len) {
    assert(len <= readableBytes());
    readIndex += len;
  }

  void retrieveUntil(const char* end) {
    assert(peek()<=end);
    retrieve(end - peek());
  }

  void retrieveAll() {
    //全部数据读完，两个指针需要返回原位以备新一轮使用
    readIndex = kCheapPrepend;
    writeIndex = kCheapPrepend;
  }

  //读取所有数据并转化为字符串
  std::string retrieveAsString() {
    std::string str (peek(),readableBytes());
    retrieveAll();
    return str;
  }

  void append(const std::string& str) {
    append(str.data(),str.length());
  }

  void append(const char* data,size_t len ) {
    ensureWritableBytes(len);
    std::copy(data,data+len,beginWrite());
    hasWritten(len);
  }

  void ensureWritableBytes(size_t len)  {
    if(writableBytes()<len) {
      makeSpace(len);
    }
    assert(writableBytes()>=len);
  }


  //开始写的位置
  char* beginWrite() {
    return begin()+writeIndex;
  }

  const char* beginWrite() const{
    return begin()+writeIndex;
  }

  //写入数据
  void hasWritten(size_t len) {
    writeIndex+=len;
  }

  //在空闲取添加几个字节，创新点
  void prepend(const void* data,size_t len) {
    assert(len<=prependableBytes());
    readIndex -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d,d+len,begin()+readIndex);
  }

  void shrink(size_t reserve)
  {
    std::vector<char> buf(kCheapPrepend+readableBytes()+reserve);
    std::copy(peek(), peek()+readableBytes(), buf.begin()+kCheapPrepend);
    buf.swap(buffer_);
  }

  ssize_t readfd(int fd,int* savedErrno);

private:
  char* begin() {
    return &*buffer_.begin();
  }

  const char* begin() const {
    return &*buffer_.begin();
  }

  //自动增长
  void makeSpace(size_t len) {
    if(writableBytes()+prependableBytes()<len+kCheapPrepend) {
      buffer_.resize(writeIndex+len);
    }else { //重置两个index
      assert(kCheapPrepend<readIndex);
      size_t readable = readableBytes();
      std::copy(begin()+readIndex,begin()+writeIndex,begin()+kCheapPrepend);
      readIndex = kCheapPrepend;
      writeIndex = readIndex + readable;
      assert(readable == readableBytes());
    }
  }
  std::vector<char> buffer_;
  size_t readIndex;
  size_t writeIndex;
};
}

#endif //BUFFER_H
