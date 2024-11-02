

#ifndef ATOMIC_H
#define ATOMIC_H
namespace muduo
{

namespace detail
{
template<typename T>
class AtomicIntegerT
{
public:
  // 构造函数，初始化原子变量为0
  AtomicIntegerT()
    : value_(0)
  {
  }

  // 获取当前值
  T get() const
  {
    // 使用 __sync_val_compare_and_swap 实现原子获取
    return __sync_val_compare_and_swap(const_cast<volatile T*>(&value_), 0, 0);
  }

  // 获取当前值并增加 x
  T getAndAdd(T x)
  {
    return __sync_fetch_and_add(&value_, x);
  }

  // 增加 x 并返回新值
  T addAndGet(T x)
  {
    return getAndAdd(x) + x;
  }

  // 自增并返回新值
  T incrementAndGet()
  {
    return addAndGet(1);
  }

  // 增加 x
  void add(T x)
  {
    getAndAdd(x);
  }

  // 自增
  void increment()
  {
    incrementAndGet();
  }

  // 自减
  void decrement()
  {
    getAndAdd(-1);
  }

  // 设置新值并返回旧值
  T getAndSet(T newValue)
  {
    return __sync_lock_test_and_set(&value_, newValue);
  }

private:
  volatile T value_;  // 原子变量，使用 volatile 保证其在多线程环境中的可见性
};

}

// 定义 32 位和 64 位的原子整数类型
typedef detail::AtomicIntegerT<int> AtomicInt32;
typedef detail::AtomicIntegerT<int> AtomicInt64;

}
#endif //ATOMIC_H
