// (c) 2022. Taras Mykhailovych. "Prywit Research Labs"
// `pw-async`: PryWit - ASYNChronous iterators
// C99+ Language Header File
// Description:
//   An extension of Prywit Iterators to allow execution of async code in a generator function.
// How?

#ifndef __PW_ASYNC__
#define __PW_ASYNC__

#include <sys/types.h>
#include <time.h>

#include "pw-iter.h"

#if ( defined _WIN32 || defined __WIN32__ ) && ! defined __CYGWIN__
  #include <winsock2.h>
  #include <windows.h>
  #define poll WSAPoll
#else
  #include <poll.h>
  #include <unistd.h>
#endif

// pwa task -- awaiting event or issue a command to event loop
#define pwa_Task_await_fd  0
#define pwa_Task_delay     1
#define pwa_Task_async_job 2
#define pwa_Task_hit_job   3
#define pwa_Task_hit_all_jobs  4

// is set, when iterator is managed by event loop
#define pwa_Task_attached_bit ((long long)1 << 41)
#define pwa_Task_await_bit ((long long)1 << 42)
#define pwa_Task_await_shift 48
#define pwa_Task_await_mask ((1 << 8) - 1)
#define pwa_Task_await_mask_shifted ((long long) pwa_Task_await_mask << pwa_Task_await_shift)
#define pwa_Task_await_clear (~(pwa_Task_await_mask_shifted | pwa_Task_await_bit))

#define pwa_Task_await_save_bits ( \
  ((long long) -_pwi_state_final_bit) - \
  ((long long) pwa_Task_await_mask << pwa_Task_await_shift) - \
  pwa_Task_await_bit \
)

// types

typedef pwi_Iterator_Func pwa_Iterator_Func;
typedef pwi_Iterator pwa_Iterator;

#define pwa_iterator pwi_iterator

typedef struct pwa_Task_AwaitFd {
  pwa_Iterator *iterator;
  struct pollfd *fds;
} pwa_Task_AwaitFd;

typedef struct pwa_Task_Delay {
  pwa_Iterator *iterator;
  struct timespec until;
} pwa_Task_Delay;

typedef struct pwa_Task_HitJob {
  pwa_Iterator *iterator;
  char how;
} pwa_Task_HitJob;

#define pwa_Task_hit_detach 0
#define pwa_Task_hit_halt 1
#define pwa_Task_hit_finish 2
#define pwa_Task_hit_force_finish 3
#define pwa_Task_hit_force_next 4
#define pwa_Task_hit_kill -1

typedef struct pwa_EventLoop {
  int nTasks, nTaskAlloc;
  int nDelays, nDelayAlloc;
  struct pollfd *fds;
  pwa_Task_AwaitFd *tasks;
  pwa_Task_Delay *delays;
} pwa_EventLoop;

// helpers

static inline void pwa_timespec_diff(struct timespec* dst, struct timespec* a, struct timespec* b) {
  dst->tv_nsec = a->tv_nsec - b->tv_nsec;
  dst->tv_sec = a->tv_sec - b->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline double pwa_timespec_diff_sec(struct timespec* a, struct timespec* b) {
  struct timespec dst;
  pwa_timespec_diff(&dst, a, b);
  return dst.tv_sec + ((double) dst.tv_nsec / 1e9);
}

static inline int pwa_timespec_cmp(struct timespec* a, struct timespec* b) {
  if (a->tv_sec < b->tv_sec) return -1;
  if (a->tv_sec > b->tv_sec) return 1;
  if (a->tv_nsec < b->tv_nsec) return -1;
  if (a->tv_nsec > b->tv_nsec) return 1;
  return 0;
}

static inline void pwa_timespec_add(struct timespec* dst, struct timespec* src) {
  dst->tv_nsec += src->tv_nsec;
  dst->tv_sec += src->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline void pwa_timespec_sub(struct timespec* dst, struct timespec* src) {
  dst->tv_nsec -= src->tv_nsec;
  dst->tv_sec -= src->tv_sec;
  if (dst->tv_nsec < 0) { dst->tv_nsec += 1e9; --dst->tv_sec; }
  else if (dst->tv_nsec >= 1e9) { dst->tv_nsec -= 1e9; ++dst->tv_sec; }
}

static inline void pwa_timespec_add_sec(struct timespec *dst, double sec) {
  struct timespec src;
  src.tv_sec = (time_t) sec;
  src.tv_nsec = (long) ((sec - src.tv_sec) * 1e9);
  pwa_timespec_add(dst, &src);
}

static inline struct timespec * pwa_timespec_monoClockIn(struct timespec *dst, double sec) {
  int err = clock_gettime(CLOCK_MONOTONIC, dst);
  if (err) return 0;
  if (sec == 0.0) return dst;
  pwa_timespec_add_sec(dst, sec);
  return dst;
}

// macros

#define pwa_func(type, name, args, vars) \
  pwi_func(type, name, args, ( \
    struct pollfd _pwa_fds; \
    struct timespec _pwa_until; \
    pwa_Task_HitJob _pwa_hit; \
    _pw_multi vars \
  ))

#define pwa_finally pwi_finally
#define pwa_end_func pwi_end_func

#define pwa_iterate pwi_iterate
#define pwa_iterate_var pwi_iterate_var

#define pwa_reset pwi_reset

#define pwa_yield pwi_yield
#define pwa_return pwi_return
#define pwa_throw pwi_throw

#define pwa_handled pwi_handled
#define pwa_rethrow pwi_rethrow

#define pwa_exit pwi_exit
#define pwa_shut pwi_shut

#define pwa_task_await_up_(_task, _desc, _label) { \
  _pwi_iter->state = (unsigned long long)(unsigned)(_label) | (_pwi_state & pwa_Task_await_save_bits) | \
    (_task); \
  _pwi_iter->tag = (void *) (_desc); \
  return _pwi_iter; case (_label): \
  _pwi_iter->state = ((unsigned)(int) _pwi_state_final) | (_pwi_state & pwa_Task_await_save_bits); \
}
#if defined __COUNTER__
  #define pwa_task_await_up(_task, _desc) pwa_task_await_up_(_task, _desc, __COUNTER__ + 1)
#elif defined __LINE__
  #define pwa_task_await_up(_task, _desc) pwa_task_await_up_(_task, _desc, __LINE__)
#else
  #error "compiler must have __COUNTER__ or __LINE__ capability"
#endif

#define pwa_task_await(_task, _desc) pwa_task_await_up( \
  pwa_Task_await_bit | ((unsigned long long)(_task) << pwa_Task_await_shift), \
  _desc \
)

#define pwa_await_fd(_fd, _events) { \
  _->_pwa_fds.fd = _fd; \
  _->_pwa_fds.events = _events; \
  _->_pwa_fds.revents = 0; \
  pwa_task_await(pwa_Task_await_fd, &(_->_pwa_fds)) \
}

#define pwa_await_fd_res(_revents, _fd, _events) \
  pwa_await_fd(_fd, _events) \
  _revents = _->_pwa_fds.revents

#define pwa_delay(_sec) { \
  if (pwa_timespec_monoClockIn(&_->_pwa_until, (double) (_sec))) \
    pwa_task_await(pwa_Task_delay, &_->_pwa_until) \
}

#define pwa_async_job(_iter) pwa_task_await(pwa_Task_async_job, &(_iter))

#define pwa_job_hit(_iter, _how) { \
  _->_pwa_hit.iterator = (pwa_Iterator *) &(_iter); \
  _->_pwa_hit.how = _how; \
  pwa_task_await(pwa_Task_hit_job, &_->_pwa_hit) \
}

#define pwa_job_detach(_iter) pwa_job_hit(_iter, pwa_Task_hit_detach)
#define pwa_job_halt(_iter) pwa_job_hit(_iter, pwa_Task_hit_halt)
#define pwa_job_finish(_iter) pwa_job_hit(_iter, pwa_Task_hit_finish)
#define pwa_job_force_finish(_iter) pwa_job_hit(_iter, pwa_Task_hit_force_finish)
#define pwa_job_force_next(_iter) pwa_job_hit(_iter, pwa_Task_hit_force_next)
#define pwa_job_kill(_iter) pwa_job_hit(_iter, pwa_Task_hit_kill)

#define pwa_all_jobs_hit(_how) \
  pwaTaskAwait(pwa_Task_hit_all_jobs, (ssize_t) (_how))

#define pwa_all_jobs_detach() pwa_all_jobs_hit(pwa_Task_hit_detach)
#define pwa_all_jobs_halt() pwa_all_jobs_hit(pwa_Task_hit_halt)
#define pwa_all_jobs_finish() pwa_all_jobs_hit(pwa_Task_hit_finish)
#define pwa_all_jobs_force_finish() pwa_all_jobs_hit(pwa_Task_hit_force_finish)
#define pwa_all_jobs_force_next() pwa_all_jobs_hit(pwa_Task_hit_force_next)
#define pwa_all_jobs_kill() pwa_all_jobs_hit(pwa_Task_hit_kill)

#define pwa_iter_await(_iter, _arg) { \
  while ((_iter).state & pwa_Task_await_bit) { \
    pwa_task_await_up((_iter).state & pwa_Task_await_mask_shifted, (_iter).tag); \
    (_iter).state &= ~pwa_Task_await_bit; \
    pwi_next_(_iter, _arg); \
  } \
}

#define pwa_sync_(_iter, _arg) { \
  if (!((_iter).state & (pwa_Task_attached_bit | _pwi_state_done_bit))) { \
    (_iter).state |= pwa_Task_attached_bit; \
    pwa_iter_await(_iter, _arg) \
  } \
}
#define pwa_sync(_iter) pwa_sync_(_iter, 0)

#define pwa_sync_job_(_iter, _arg) { \
  pwa_job_detach(_iter); \
  pwa_sync_(_iter, _arg); \
}
#define pwa_sync_job(_iter) pwa_sync_job_(_iter, 0)

#define pwa_upstream(_iter, _arg, _body) { \
  if (!((_iter).state & pwa_Task_await_bit)) { \
    _pw_multi _body; \
    pwa_iter_await(_iter, _arg) \
  } \
}

#define pwa_next_(_iter, _value) \
  pwa_upstream(_iter, _value, (pwi_next_(_iter, _value)))
#define pwa_next(id) pwa_next_(id, 0)

#define pwa_halt_(_iter, _value) \
  pwa_upstream(_iter, _value, (pwi_halt_(_iter, _value)))
#define pwa_halt(id) pwa_halt_(id, 0)

#define pwa_finish_(_iter, _value) \
  pwa_upstream(_iter, _value, (pwi_finish_(_iter, _value)))
#define pwa_finish(id) pwa_finish_(id, 0)

#define pwa_kill_ pwi_kill_
#define pwa_kill pwi_kill

#define pwa_fail_(_iter, _error, _value) \
  pwa_upstream(_iter, _value, (pwi_fail_(_iter, _error, _value)))
#define pwa_fail(id, _error) pwa_fail_(id, _error, 0)

#define pwa_exec_(_iter, _arg) { \
  while (!(_iter).done) { pwa_next_(_iter, _arg); } \
}
#define pwa_exec(_iter) pwa_exec_(_iter, 0)

#define pwa_finish_exec_(_iter, _arg) { \
  while (!(_iter).done) { pwa_finish_(_iter, _arg); } \
}
#define pwa_finish_exec(_iter) pwa_finish_exec_(_iter, 0)

#define pwa_yields_(_iter, _arg) { \
  while (1) { \
    pwa_next_(_iter, _arg); \
    if ((_iter).done) break; \
    pwa_yield((_iter).value); \
  } \
}
#define pwa_yields(_iter) pwa_yields_(_iter, 0)

#define pwa_throws pwi_throws
#define pwa_returns pwi_returns
#define pwa_exits pwi_exits

#define pwa_next_s(_iter) { pwa_next(_iter); pwa_throws(_iter) }
#define pwa_finish_s(_iter) { pwa_finish(_iter); pwa_throws(_iter) }
#define pwa_fail_s(_iter) { pwa_fail(_iter); pwa_throws(_iter) }
#define pwa_halt_s(_iter) { pwa_halt(_iter); pwa_throws(_iter) }

#define pwa_sync_exec_(_iter, _arg) { \
  pwa_sync_job_(_iter, _arg); \
  pwa_exec_(_iter, _arg); \
}
#define pwa_sync_exec(_iter) pwa_sync_exec_(_iter, 0)

#define pwa_sync_yields_(_iter, _arg) { \
  pwa_sync_job(_iter, _arg); \
  pwa_yields(_iter, _arg); \
}
#define pwa_sync_yields(_iter) pwa_sync_yields_(_iter, 0)

#define pwa_for_(_var, _iter, _arg) { \
  while (1) { \
    pwa_next_(_iter, _arg); \
    if ((_iter).done) break; \
    _var = (typeof((_iter).value)) (_iter).value;
#define pwa_end_for(_iter) \
  } pwa_finish_exec(_iter); \
}
#define pwa_for(_var, _iter) pwa_for_(_var, _iter, 0)

#define pwa_for_s pwa_for
#define pwa_end_for_s(_iter) pwa_end_for(_iter) pwa_throws(_iter)

// errors

#define pwa_errors pwi_errors
#define pwa_error pwi_error
#define pwa_is_error pwi_is_error
#define pwa_catch pwi_catch
#define pwa_catch_all pwi_catch_all
#define pwa_type_str pwi_type_str
#define pwa_error_str pwi_error_str
#define pwa_thrown pwi_thrown
#define pwa_thrown_str pwi_thrown_str

// event loop lifecycle

void pwa_EventLoop_init(pwa_EventLoop *loop);
#define pwa_loop_init(id) \
  pwa_EventLoop_init(&(id))
#define pwa_loop_init_var(id) \
  pwa_EventLoop id; \
  pwa_loop_init(id)

void pwa_EventLoop_addAsync(pwa_EventLoop *loop, pwa_Iterator *iter, void* arg);
#define pwa_loop_async_job(_loop, _iter) \
  pwa_EventLoop_addAsync(&(_loop), (pwa_Iterator *) &(_iter), 0)

int pwa_EventLoop_hitJob(pwa_EventLoop *loop, pwa_Iterator *ignored, pwa_Task_HitJob *hit);
#define pwa_loop_job_hit(_loop, _iter, _how) \
  pwa_EventLoop_hitJob(&(_loop), 0, &(pwa_Task_HitJob) { .iterator = (pwa_Iterator *) &(_iter), .how = (_how) })

#define pwa_loop_job_detach(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_detach)
#define pwa_loop_job_Halt(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_halt)
#define pwa_loop_job_finish(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_finish)
#define pwa_loop_job_force_finish(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_force_finish)
#define pwa_loop_job_force_next(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_force_next)
#define pwa_loop_job_force_kill(_loop, _iter) pwa_loop_job_hit(_loop, _iter, pwa_Task_hit_kill)

int pwa_EventLoop_hitAllJobs(pwa_EventLoop *loop, pwa_Iterator *ignored, ssize_t how);
#define pwa_loop_all_jobs_hit(_loop, _how) \
  pwa_EventLoop_hitAllJobs(&(_loop), 0, _how)

#define pwa_loop_all_jobs_detach(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_detach)
#define pwa_loop_all_jobs_halt(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_halt)
#define pwa_loop_all_jobs_finish(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_finish)
#define pwa_loop_all_jobs_force_finish(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_forceFinish)
#define pwa_loop_all_jobs_force_next(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_forceNext)
#define pwa_loop_all_jobs_kill(_loop) pwa_loop_all_jobs_hit(_loop, pwa_Task_hit_kill)

ssize_t pwa_EventLoop_run(pwa_EventLoop *loop);
#define pwa_loop_run(_loop) \
  pwa_EventLoop_run(&(_loop))

void pwa_EventLoop_free(pwa_EventLoop *loop);
#define pwa_loop_free(id) \
  pwa_EventLoop_free(&(id))

int pwa_App_handleSignals(int n, int* signals, void (*handler)(int));
#define pwa_on_signals(_signals, _handler) { \
  int signals[] = { _pw_multi _signals }; \
  pwa_App_handleSignals(sizeof(signals) / sizeof(int), signals, _handler); \
}

int pwa_App_handleExitSignals(void (*handler)(int));
#define pwa_on_exit_signals(_handler) pwa_App_handleExitSignals(_handler)

#endif
