cmake_minimum_required(VERSION 3.28.3)
project(muduo_cpp11_std)

# 设置默认编译类型
set(CMAKE_BUILD_TYPE "Debug")
# 设定C++标准和编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread")

# 启用测试
enable_testing()

# 包含头文件目录
include_directories("${PROJECT_SOURCE_DIR}/muduo")

# 添加 Muduo 库的源文件
add_library(muduo
        muduo/TimeStamp.cpp
        muduo/TimerQueue.cpp
        muduo/Timer.cpp
        muduo/SocketsOps.cpp
        muduo/Poll.cpp
        muduo/EventLoopThreadPool.cpp
        muduo/EventLoopThread.cpp
        muduo/EventLoop.cpp
        muduo/EPoll.cpp
        muduo/Channel.cpp
        muduo/Callbacks.cpp
        muduo/Buffer.cpp
        muduo/thread/Thread.cpp
        muduo/net/Acceptor.cpp
        muduo/net/Connector.cpp
        muduo/net/InetAddress.cpp
        muduo/net/Socket.cpp
        muduo/net/TcpClient.cpp
        muduo/net/TcpConnection.cpp
        muduo/net/TcpServer.cpp
        muduo/log/base/AsyncLogging.cpp
        muduo/log/base/CountDownLatch.cpp
        muduo/log/base/FileUtil.cpp
        muduo/log/base/LogFile.cpp
        muduo/log/base/Logging.cpp
        muduo/log/base/LogStream.cpp
        muduo/log/base/Thread.cpp
)

# 添加测试文件
set(TEST_SOURCES
        muduo/test/test1.cc
        muduo/test/test2.cc
        muduo/test/test3.cc
        muduo/test/test4.cc
        muduo/test/test5.cc
        muduo/test/test6.cc
        muduo/test/test7.cc
        muduo/test/test8.cc
        muduo/test/test9.cc
        muduo/test/test10.cc
        muduo/test/test11.cc
        muduo/test/test12.cc
        muduo/test/test13.cc
)

# 为每个测试文件添加可执行文件
foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} muduo)
endforeach()

# 设置输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/)

