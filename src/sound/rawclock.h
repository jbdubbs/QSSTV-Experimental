#ifndef RAWCLOCK_H
#define RAWCLOCK_H

#include <chrono>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

/*!
  Seconds from a monotonic clock that is not steered by NTP/chrony (CLOCK_MONOTONIC_RAW, the performance
  counter on Windows). The NTP calibration measures both the soundcard and the network time against this
  one clock, so its own crystal error cancels out of the result.
*/
inline double rawMonotonicSeconds()
{
#if defined(_WIN32)
  static LARGE_INTEGER freq={};
  if(freq.QuadPart==0) QueryPerformanceFrequency(&freq);
  LARGE_INTEGER c;
  QueryPerformanceCounter(&c);
  return double(c.QuadPart)/double(freq.QuadPart);
#elif defined(CLOCK_MONOTONIC_RAW)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW,&ts);
  return double(ts.tv_sec)+1e-9*double(ts.tv_nsec);
#else
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
#endif
}

#endif // RAWCLOCK_H
