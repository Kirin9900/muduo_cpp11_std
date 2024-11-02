

#ifndef SOCKET_H
#define SOCKET_H


namespace muduo {
class InetAddress;

class Socket {
public:
  Socket(int sockfd):sockfd_(sockfd){}
  ~Socket();

  int fd() const {return sockfd_;}
  void bindAddress(const InetAddress& localddr);
  void listen();

  int accept(InetAddress* peeraddr);

  void setReuseAddr(bool on);

  void shutdownWrite();

  void setTcpNoDelay(bool on);
private:
  const int sockfd_;
};

}

#endif //SOCKET_H
