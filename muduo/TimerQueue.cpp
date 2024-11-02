#include "TimerQueue.h"

#include "../muduo/log/base/Logging.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

#include <functional>      // 使用 std::bind
#include <unistd.h>        // 用于 ::close
#include <sys/timerfd.h>   // 定时器文件描述符相关函数
#include <cstring>         // 用于 bzero

namespace muduo
{
    namespace net
    {
        namespace detail
        {


            int createTimerfd()
            {
                int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
                if (timerfd < 0)
                {
                    LOG << "Failed in timerfd _create";
                }

                return timerfd;
            }

            struct timespec howMuchTimeFromNow(Timestamp when)
            {
                int64_t microseconds = when.microSecondsSinceEpoch().count() -
                                       Timestamp::now().microSecondsSinceEpoch().count();
                if (microseconds < 100)
                    microseconds = 100;

                struct timespec ts;

                ts.tv_sec = static_cast<time_t >
                (microseconds / Timestamp::kMicroSecondsPerSecond);
                ts.tv_nsec = static_cast<long >
                             (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000;
                return ts;
            }


            void readTimerfd(int timerfd, Timestamp now)
            {
                uint64_t howmany;
                ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
                LOG << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
                if (n != sizeof howmany)
                    LOG << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
            }

            void resetTimerfd(int timerfd, Timestamp expiration)
            {
//                /* POSIX.1b structure for timer start values and intervals.  */
//                struct itimerspec
//                {
//                    struct timespec it_interval;
//                    struct timespec it_value;
//                };
                struct itimerspec newValue;
                struct itimerspec oldValue;

                bzero(&newValue, sizeof newValue);
                bzero(&oldValue, sizeof oldValue);

                newValue.it_value = howMuchTimeFromNow(expiration);
                int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
                if (ret)
                    LOG << " timerfd_settime() ";
            }
        }
    }
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop *loop)
        : loop_(loop), timerfd_(createTimerfd()),
          timerfdChannel_(loop, timerfd_),
          timers_(),
          callingExpiredTimers_(false)
{
    timerfdChannel_.setWReadCallback(
            std::bind(&TimerQueue::handleRead, this));
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    timerfdChannel_.disableAll();
    
    ::close(timerfd_);
    for (auto it = timers_.begin(); it != timers_.end(); ++it)
    {
        delete it->second;
    }
}

TimerId TimerQueue::addTimer(const TimerCallback &cb, Timestamp when, double interval)
{
    Timer *timer = new Timer(cb, when, interval);
    loop_->runInLoop(std::bind(&TimerQueue::addTimerInLoop, this, timer));

    return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(
            std::bind(&TimerQueue::cancelInLoop, this, timerId)
    );
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged)
        resetTimerfd(timerfd_, timer->expiration());
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end())
    {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        delete it->first;
        activeTimers_.erase(it);
    } else if (callingExpiredTimers_)
    {
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);
    std::vector<Entry> expired = getExpired(now);
    callingExpiredTimers_ = true;
    cancelingTimers_.clear();

    for (auto it = expired.begin(); it != expired.end(); ++it)
    {
        it->second->run();
    }
    callingExpiredTimers_ = false;
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));//18446744073709551615UL
    auto end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);
    for (auto it = expired.begin(); it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
    }
    assert(timers_.size() == activeTimers_.size());
    return expired;
}


void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextExpire;
    for (auto it = expired.begin(); it != expired.end(); ++it)
    {
        ActiveTimer timer(it->second, it->second->sequence());
        if (it->second->repeat()
            && cancelingTimers_.find(timer) == cancelingTimers_.end()
                )
        {
            it->second->restart(now);
            insert(it->second);
        } else
        {
            delete it->second;
        }
    }

    if (!timers_.empty())
    {
        nextExpire = timers_.begin()->second->expiration();
    }
    if (nextExpire.valid())
    {
        resetTimerfd(timerfd_, nextExpire);
    }

}
bool TimerQueue::insert(Timer *timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size()==activeTimers_.size());
    bool earliestChanged=false;
    Timestamp when=timer->expiration();
    TimerList::iterator it=timers_.begin();
    if(it==timers_.end()|| when<it->first)
        earliestChanged=true;

    {
        auto result=timers_.insert(Entry(when,timer));
        assert(result.second);
    }
    {
        auto result= activeTimers_.insert(ActiveTimer(timer,timer->sequence()));
        assert(result.second);
    }
    return earliestChanged;
}