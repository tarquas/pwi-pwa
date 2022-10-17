#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include "pw-async.h"

// event loop implementation

void pwa_EventLoop_init(pwa_EventLoop *loop) {
  int nAlloc = loop->nTaskAlloc = loop->nDelayAlloc = getpagesize();
  loop->fds = (struct pollfd *) malloc(nAlloc * sizeof(struct pollfd));
  loop->tasks = (pwa_Task_AwaitFd *) malloc(nAlloc * sizeof(pwa_Task_AwaitFd));
  loop->delays = (pwa_Task_Delay *) malloc(nAlloc * sizeof(pwa_Task_Delay));
  loop->nTasks = loop->nDelays = 0;
}

void pwa_EventLoop_free(pwa_EventLoop *loop) {
  free(loop->delays);
  free(loop->tasks);
  free(loop->fds);
}

int pwa_EventLoop_addTask(pwa_EventLoop *loop, pwa_Iterator *iterator, struct pollfd *fds) {
  if (loop->nTasks == loop->nTaskAlloc) {
    int nTaskAlloc = loop->nTaskAlloc += getpagesize();
    loop->fds = (struct pollfd *) realloc(loop->fds, nTaskAlloc * sizeof(struct pollfd));
    loop->tasks = (pwa_Task_AwaitFd *) realloc(loop->tasks, nTaskAlloc * sizeof(pwa_Task_AwaitFd));
  }
  int taskId = loop->nTasks++;
  loop->fds[taskId] = *fds;
  loop->tasks[taskId] = (pwa_Task_AwaitFd) { iterator, fds };
  return 1;
}

int pwa_EventLoop_removeTask(pwa_EventLoop *loop, int taskId) {
  if (taskId < 0 || taskId >= loop->nTasks) return 0;
  int lastTaskId = --loop->nTasks;
  if (taskId != lastTaskId) {
    loop->fds[taskId] = loop->fds[lastTaskId];
    loop->tasks[taskId] = loop->tasks[lastTaskId];
  }
  return 1;
}

int pwa_EventLoop_addDelay(pwa_EventLoop *loop, pwa_Iterator *iterator, struct timespec *until) {
  if (loop->nDelays == loop->nDelayAlloc) {
    int nDelayAlloc = loop->nDelayAlloc += getpagesize();
    loop->delays = (pwa_Task_Delay *) realloc(loop->delays, nDelayAlloc * sizeof(pwa_Task_Delay));
  }
  int delayId = loop->nDelays++;
  pwa_Task_Delay *delay = loop->delays + delayId;
  delay->iterator = iterator;
  delay->until = *until;
  return 1;
}

int pwa_EventLoop_removeDelay(pwa_EventLoop *loop, int delayId) {
  if (delayId < 0 || delayId >= loop->nDelays) return 0;
  int lastDelayId = --loop->nDelays;
  if (delayId != lastDelayId) {
    loop->delays[delayId] = loop->delays[lastDelayId];
  }
  return 1;
}

int pwa_EventLoop_action_async(pwa_EventLoop *, pwa_Iterator *, pwa_Iterator *);
static void _pwa_EventLoop_addJob(pwa_EventLoop *, pwa_Iterator *, void *);

int pwa_EventLoop_hitIter(pwa_EventLoop *loop, pwa_Iterator *iter, char how) {
  switch (how) {
    case pwa_Task_hit_detach: iter->state &= ~pwa_Task_attached_bit; return 0;
    case pwa_Task_hit_force_finish: iter->state &= pwa_Task_await_clear;
      if (!(iter->state & _pwi_state_final_bit)) *(int *)iter->state = (int) _pwi_state_final; break;
    case pwa_Task_hit_finish: if (!(iter->state & _pwi_state_final_bit)) {
      iter->state = (iter->state & (pwa_Task_await_clear - _pwi_state_final_bit)) | (unsigned)(int) _pwi_state_final;
    } break;
    case pwa_Task_hit_kill:
      iter->state = (unsigned)(int) _pwi_state_final | _pwi_state_final_bit | _pwi_state_done_bit; break;
    case pwa_Task_hit_halt: *(int *)&iter->state = (int) _pwi_state_final; break;
    case pwa_Task_hit_force_next: iter->state &= pwa_Task_await_clear; break;
  }

  _pwa_EventLoop_addJob(loop, iter, 0);
  return 0;
}

int pwa_EventLoop_hitJob(pwa_EventLoop *loop, pwa_Iterator *ignored, pwa_Task_HitJob *hit) {
  if (hit->how == pwa_Task_hit_finish && hit->iterator->state & _pwi_state_final_bit) return 0;

  int n, found = 0;
  pwa_Task_AwaitFd *task = loop->tasks;
  pwa_Task_Delay *delay = loop->delays;
  pwa_Iterator *iter = hit->iterator;
  n = loop->nTasks;
  if (n) for (int i = 0; i < n; ++i, ++task) {
    if (task->iterator == iter) {
      pwa_EventLoop_removeTask(loop, i);
      found = 1;
      break;
    }
  }
  n = loop->nDelays;
  if (n && !found) for (int i = 0; i < n; ++i, ++delay) {
    if (delay->iterator == iter) {
      pwa_EventLoop_removeDelay(loop, i);
      found = 1;
      break;
    }
  }
  if (!found) return 0;
  pwa_EventLoop_hitIter(loop, iter, hit->how);
  return 0;
}

int pwa_EventLoop_hitAllJobs(pwa_EventLoop *loop, pwa_Iterator *ignored, ssize_t how) {
  int nTasks = loop->nTasks, nDelays = loop->nDelays;
  free(loop->fds);
  pwa_Task_AwaitFd* tasks = loop->tasks, *task = tasks;
  pwa_Task_Delay* delays = loop->delays, *delay = delays;
  pwa_EventLoop_init(loop);
  for (int i = 0; i < nTasks; ++i, ++task) pwa_EventLoop_hitIter(loop, task->iterator, how);
  for (int i = 0; i < nDelays; ++i, ++delay) pwa_EventLoop_hitIter(loop, delay->iterator, how);
  free(delays);
  free(tasks);
  return 0;
}

typedef int (*pwa_EventLoop_Action)(pwa_EventLoop *, pwa_Iterator *, void *);
pwa_EventLoop_Action pwa_EventLoop_actions[] = {
  [pwa_Task_await_fd] = (pwa_EventLoop_Action) pwa_EventLoop_addTask,
  [pwa_Task_delay] = (pwa_EventLoop_Action) pwa_EventLoop_addDelay,
  [pwa_Task_async_job] = (pwa_EventLoop_Action) pwa_EventLoop_action_async,
  [pwa_Task_hit_job] = (pwa_EventLoop_Action) pwa_EventLoop_hitJob,
  [pwa_Task_hit_all_jobs] = (pwa_EventLoop_Action) pwa_EventLoop_hitAllJobs,
};

static void _pwa_EventLoop_addJob(pwa_EventLoop *loop, pwa_Iterator *iter, void *arg) {
  while (1) {
    while (!(iter->state & _pwi_state_stall)) { iter->next(iter, arg); } // fast-forward until async or done
    if (!(iter->state & pwa_Task_await_bit)) { return; } // if iterator done or race condition
    pwa_EventLoop_Action action = pwa_EventLoop_actions[(iter->state >> pwa_Task_await_shift) & pwa_Task_await_mask];
    if (!action) continue; // not implemented task -- ignore
    if (action(loop, iter, iter->tag)) break; // if planned next event
    iter->state &= pwa_Task_await_clear;
  }
}

void pwa_EventLoop_addAsync(pwa_EventLoop *loop, pwa_Iterator *iter, void* arg) {
  if (iter->state & _pwi_state_done_bit) return;
  if (iter->state & pwa_Task_await_bit) {
    if (iter->state & pwa_Task_attached_bit) return;
    iter->state |= pwa_Task_attached_bit;
  }
  _pwa_EventLoop_addJob(loop, iter, arg);
}

int pwa_EventLoop_action_async(pwa_EventLoop *loop, pwa_Iterator *ignore, pwa_Iterator *iterator) {
  if (!loop) return 0;
  pwa_EventLoop_addAsync(loop, iterator, 0);
  return 0;
}

#define pwa_EventLoop_maxWaitSec 1e6
#define pwa_EventLoop_maxWaitMsec (pwa_EventLoop_maxWaitSec * 1000)

int pwa_EventLoop_getWaitTimeout(pwa_EventLoop *loop, struct timespec *span) {
  int n = loop->nDelays;
  if (!n) {
    span->tv_sec = pwa_EventLoop_maxWaitSec;
    span->tv_nsec = 0;
    return pwa_EventLoop_maxWaitMsec;
  }
  pwa_Task_Delay *delay = loop->delays;
  struct timespec now, min = delay->until;
  ++delay;
  for (int i = 1; i < n; ++i, ++delay) {
    if (pwa_timespec_cmp(&delay->until, &min) < 0) min = delay->until;
  }
  if (clock_gettime(CLOCK_MONOTONIC, &now)) { return -1; };
  pwa_timespec_diff(span, &min, &now);
  if (span->tv_sec < 0) {
    span->tv_sec = 0;
    span->tv_nsec = 0;
    return 0;
  }
  if (span->tv_sec > pwa_EventLoop_maxWaitSec) {
    span->tv_sec = pwa_EventLoop_maxWaitSec;
    span->tv_nsec = 0;
    return pwa_EventLoop_maxWaitMsec;
  }
  return span->tv_sec * 1e3 + span->tv_nsec / 1e6;
}

int pwa_EventLoop_pollEvents(pwa_EventLoop *loop, struct timespec *span, int timeoutMsec) {
  if (!timeoutMsec) return 0;
  if (!loop->nTasks) {
    if (nanosleep(span, NULL) && errno != EINTR) { return -3; }
    return 0;
  }
  int polled = poll(loop->fds, loop->nTasks, timeoutMsec);
  if (polled == -1) return errno == EINTR ? 0 : -2;
  return polled;
}

int pwa_EventLoop_execTasks(pwa_EventLoop *loop, int polled) {
  if (!polled) return 0;

  pwa_Iterator *iter;
  pwa_Task_AwaitFd *task = loop->tasks;
  struct pollfd *fds = loop->fds;
  int n = loop->nTasks, p = polled;

  for (int i = 0; p && i < n; --p, ++i, ++fds, ++task) {
    if (!fds->revents) continue;
    iter = task->iterator;
    task->fds->revents = fds->revents;
    iter->state &= pwa_Task_await_clear;
    pwa_EventLoop_removeTask(loop, i); --i; --n; --fds; --task;
    pwa_EventLoop_addAsync(loop, iter, 0);
  }
  return polled;
}

int pwa_EventLoop_execDelays(pwa_EventLoop *loop) {
  int n = loop->nDelays;
  if (!n) return 0;

  struct timespec now;
  pwa_Iterator *iter;
  pwa_Task_Delay *delay = loop->delays;
  int nRan = 0;

  if (clock_gettime(CLOCK_MONOTONIC, &now)) { return -2; };
  for (int i = 0; i < n; ++i, ++delay) {
    if (pwa_timespec_cmp(&now, &delay->until) < 0) continue;
    iter = delay->iterator;
    ++nRan;
    iter->state &= pwa_Task_await_clear;
    pwa_EventLoop_removeDelay(loop, i); --i; --n; --delay;
    pwa_EventLoop_addAsync(loop, iter, 0);
  }

  return nRan;
}

ssize_t pwa_EventLoop_run(pwa_EventLoop *loop) {
  int timeoutMsec, n;
  struct timespec span;
  ssize_t nRan = 0;

  while (loop->nTasks || loop->nDelays) {
    timeoutMsec = pwa_EventLoop_getWaitTimeout(loop, &span);
    n = pwa_EventLoop_pollEvents(loop, &span, timeoutMsec);
    if (n < 0) return n;
    n = pwa_EventLoop_execTasks(loop, n);
    if (n < 0) return n;
    nRan += n;
    n = pwa_EventLoop_execDelays(loop);
    if (n < 0) return n;
    nRan += n;
  }

  return nRan;
}

int pwa_App_handleSignals(int n, int* signals, void (*handler)(int)) {
  struct sigaction new_action, old_action;
  int *signal = signals, handled = 0;

  new_action.sa_handler = handler;
  sigemptyset (&new_action.sa_mask);
  new_action.sa_flags = 0;

  for (int i = 0; i < n; ++i, ++signal) {
    sigaction (*signal, NULL, &old_action);
    if (old_action.sa_handler == SIG_IGN) continue;
    sigaction (*signal, &new_action, NULL);
    ++handled;
  }

  return handled;
}

int exitSignals[] = { SIGINT, SIGHUP, SIGTERM };

int pwa_App_handleExitSignals(void (*handler)(int)) {
  return pwa_App_handleSignals(sizeof(exitSignals), exitSignals, handler);
}
