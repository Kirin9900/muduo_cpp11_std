# Linux环境下C++多线程高性能网络库muduo

## 项目概述

本项目采用Reactor模式，对知名linux多线程高性能网络库muduo进行重构，去除所有boost依赖，全部采用标准库模式与智能指针，并充分利用C++14新特性，使用户更容易理解与使用

**[muduo源码](https://github.com/chenshuo/muduo)**

**[项目实例](https://github.com/chenshuo/recipes)**



## 项目特点

1. 设计并实现高并发TCP网络库，利用C++11特性，处理能力提升至每秒处理XX万次连接。

2. 集成Acceptor EventThreadPoo，EventLoop,Channel,Poll等关键模块，优化网络性能，

3. 创新引入one loop per thread模型，并结合Reactor模式，增强服务器处理并发请求的效率。

4. 重构项目，摒弃Boost依赖，全面使用C++标准库，提升了项目灵活性。

5. 引入现代内存管理策略，采用智能指针确保零内存泄露，同时通过atomic类型保障数据一致性。

6. 优化缓冲区管理策略，参照Netty实践，通过双指针与snprintf提高日志记录的准确性和易读性



### 参考书籍

《linux多线程服务端编程  使用muduo网络库  》

《linux高性能服务器编程》