

#ifndef CALLBACKS_H
#define CALLBACKS_H
#include <functional>
#include <memory>
#include "Buffer.h"
#include "TimeStamp.h"

namespace muduo {

class TcpConnection;


typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
typedef std::function<void()> TimerCallback;
typedef std::function<void(const TcpConnectionPtr&)> ConnectionCallback;
typedef std::function<void (const TcpConnectionPtr&,
                              Buffer* buf,
                              Timestamp)> MessageCallback;
typedef std::function<void(const TcpConnectionPtr&)> CloseCallback;

typedef std::function<void(const TcpConnectionPtr&)> WriteCompleteCallback;


}

#endif //CALLBACKS_H

