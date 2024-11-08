#include <cstdint>
#include <cstdio>
#include "Timestamp.h"

using namespace std;
using namespace muduo;
//static_assert(sizeof(ChronoTimestamp) == sizeof(int64_t), "ChronoTimestamp == int64_t ");



string Timestamp::toString() const
{
  char buf[32] = {0};

  int64_t  us=microSecondsSinceEpoch_.count();
  int64_t seconds = us / kMicroSecondsPerSecond;
  int64_t microseconds = us % kMicroSecondsPerSecond;
  snprintf(buf, sizeof(buf) - 1, "%ld.%06ld", seconds, microseconds);
  return buf;
}

string Timestamp::toFormattedString(bool showMicroseconds) const
{
  char buf[32]={0};
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_.count()/kMicroSecondsPerSecond);
  struct tm tm_time;
  gmtime_r(&seconds, &tm_time);

  if (showMicroseconds)
  {
    int microseconds = static_cast<int>(microSecondsSinceEpoch_.count() % kMicroSecondsPerSecond);
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d",
             tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
             tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
  }
  return buf;
}