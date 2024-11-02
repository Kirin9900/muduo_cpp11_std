

#ifndef TIMESTAMP_H
#define TIMESTAMP_H

#include <string>

#include <chrono>


namespace muduo
{

    class Timestamp
    {
    public:
        Timestamp() : microSecondsSinceEpoch_(0)
        {
        }

        explicit Timestamp(std::chrono::microseconds us)
        {
            microSecondsSinceEpoch_ = us;
        }

        bool valid() const
        { return microSecondsSinceEpoch_ > std::chrono::microseconds(0); }

        void swap(Timestamp &that)
        {
            std::swap(microSecondsSinceEpoch_,that.microSecondsSinceEpoch_);
        }


        std::string toString() const;

        std::string toFormattedString(bool showMicroseconds=true) const;

        std::chrono::microseconds microSecondsSinceEpoch()
        {
            return microSecondsSinceEpoch_;
        }
        static Timestamp now()
        {
            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

            std::chrono::microseconds nowin_us=std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
            return Timestamp(nowin_us);
        };


        static Timestamp invalid()
        {
            return Timestamp();
        }

        static const int kMicroSecondsPerSecond = 1000 * 1000;
    private:
        std::chrono::microseconds microSecondsSinceEpoch_;
    };



    inline bool operator<(Timestamp lhs,Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch().count()<rhs.microSecondsSinceEpoch().count();
    }

    inline bool operator==(Timestamp lhs,Timestamp rhs)
    {
        return lhs.microSecondsSinceEpoch().count()==rhs.microSecondsSinceEpoch().count();
    }

    inline double timeDifference(Timestamp high,Timestamp low)//return :seconds
    {
        int64_t  diff=high.microSecondsSinceEpoch().count()-low.microSecondsSinceEpoch().count();

        return static_cast<double>(diff)/Timestamp::kMicroSecondsPerSecond;
    }
    inline Timestamp addTime(Timestamp timestamp, double seconds)
    {
        int64_t delta= static_cast<int64_t >(seconds* Timestamp::kMicroSecondsPerSecond);
        return Timestamp(std::chrono::microseconds((timestamp.microSecondsSinceEpoch().count()+delta)));
    }
}

#endif //TIMESTAMP_H
