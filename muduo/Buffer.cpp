

#include "Buffer.h"
#include "SocketsOps.h"

#include <cerrno>
#include <memory.h>
#include <sys/uio.h>

using namespace muduo;

ssize_t Buffer::readfd(int fd,int* savedErrno){
  char extrabuf[65536];
  struct iovec vec[2];
  const size_t writable = writableBytes();
  vec[0].iov_base = begin() + writeIndex;
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;
  const ssize_t n =readv(fd,vec,2);
  if(n<0) {
    *savedErrno = errno;
  }else if(n<=writable) {
    writeIndex += n;
  }else {
    writeIndex = buffer_.size();
    append(extrabuf,n-writable);
  }
  return n;
}