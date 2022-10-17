#ifndef __PW_TIME__
#define __PW_TIME__

#include <time.h>

static inline void pwTimespecAdd(struct timespec* dst, struct timespec* src) {
  dst->tv_nsec += src->tv_nsec;
  dst->tv_sec += src->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline void pwTimespecSub(struct timespec* dst, struct timespec* src) {
  dst->tv_nsec -= src->tv_nsec;
  dst->tv_sec -= src->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline void pwTimespecDiff(struct timespec* dst, struct timespec* a, struct timespec* b) {
  dst->tv_nsec = a->tv_nsec - b->tv_nsec;
  dst->tv_sec = a->tv_sec - b->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline int pwTimespecCmp(struct timespec* a, struct timespec* b) {
  if (a->tv_sec < b->tv_sec) return -1;
  if (a->tv_sec > b->tv_sec) return 1;
  if (a->tv_nsec < b->tv_nsec) return -1;
  if (a->tv_nsec > b->tv_nsec) return 1;
  return 0;
}

static inline void pwTimespecAddSec(struct timespec *dst, double sec) {
  struct timespec src;
  src.tv_sec = (time_t) sec;
  src.tv_nsec = (long) ((sec - src.tv_sec) * 1e9);
  pwTimespecAdd(dst, &src);
}

static inline double pwTimespecDiffSec(struct timespec* a, struct timespec* b) {
  struct timespec dst;
  pwTimespecDiff(&dst, a, b);
  return dst.tv_sec + ((double) dst.tv_nsec / 1e9);
}

static inline struct timespec * pwTimespecMonoClockIn(struct timespec *dst, double sec) {
  int err = clock_gettime(CLOCK_MONOTONIC, dst);
  if (err) return 0;
  if (sec == 0.0) return dst;
  pwTimespecAddSec(dst, sec);
  return dst;
}

#endif
