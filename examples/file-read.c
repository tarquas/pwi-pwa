#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>

#include "pw-async.h"

//

#include "pw-async.c"

pwa_EventLoop mainLoop;

#define ReadFd_defaultChunkSize 256

pwa_errors(ReadFile,
  (success, open, read, poll),
  ("success", "error: open", "error: read", "error: poll: not ready for reading")
)

typedef struct ReadFdChunk {
  char *buf;
  int size;
} ReadFdChunk;

pwa_func((ReadFdChunk), ReadFd, (int fd; int chunkSize), (
  ssize_t i, nRead, total;
  char *buf;
)) {
  if (!_->chunkSize) _->chunkSize = ReadFd_defaultChunkSize;
  _->buf = (char *) malloc((_->chunkSize + 1) * sizeof(char));
  _->total = 0;
  do {
    do {
      _->nRead = read(_->fd, _->buf, _->chunkSize);
      if (!_->nRead) { pwa_return({0}); }
      if (_->nRead == -1) {
        if (errno == EAGAIN) break;
        pwa_throw(pwa_error(ReadFile, read));
      }
      _->total += _->nRead;
      _->buf[_->nRead] = 0;
      pwa_yield(((ReadFdChunk) { _->buf, _->nRead }));
    } while(_->nRead > 0);

    pwa_await_fd_res(int res, _->fd, POLLIN);
    if (!(res & POLLIN)) { pwa_throw(pwa_error(ReadFile, poll)); }
  } while(1);
} pwa_finally {
  if (_->buf) { free(_->buf); _->buf = 0; }
} pwa_end_func

pwa_func((ReadFdChunk), ReadFile, (char *name), (
  int fd;
  ReadFd file;
)) {
  _->fd = open(_->name, O_RDONLY | O_NONBLOCK);
  if (_->fd == -1) { pwa_throw(pwa_error(ReadFile, open)); }
  _->file = pwa_iterate(ReadFd, (.fd = _->fd));
  pwa_yields(_->file);
  pwa_return(_->file.value);
} pwa_finally {
  if (_->fd) { close(_->fd); _->fd = 0; }
} pwa_end_func

pwa_func((char), MemChars, (pwa_Iterator *iter; char **buf; int *size; char partial; char calm;), (
  int i;
)) {
  while (1) {
    pwa_next(*_->iter);
    if (_->iter->done) break;
    for (_->i = 0; _->i < *_->size; ++_->i) { pwa_yield((*_->buf)[_->i]); }
  }
  if (!_->calm) { pwa_throws(*_->iter); }
} pwa_finally {
  if (!_->partial) { pwa_halt(*_->iter); }
} pwa_end_func

#define ReadFdChunkChars(_iter) \
  .iter = (pwa_Iterator *) &(_iter), \
  .buf = &((_iter).value.buf), \
  .size = &((_iter).value.size)

pwa_func((int), BackgroundJob, (int n; float delaySec; char* spam), (
  int i;
)) {
  for (_->i = 0; _->i < _->n; ++_->i) {
    pwa_delay(_->delaySec);
    printf("%s\n", _->spam);
  }
} pwa_finally {
  printf("job %s finishing...\n", _->spam);
  pwa_delay(1);
  printf("job %s finished!\n", _->spam);
} pwa_end_func

pwa_func((int), Main, (), (
  BackgroundJob job, job2;
  int i;
  char c;
  ReadFile file;
  MemChars chars;
)) {
  _->file = pwa_iterate(ReadFile, ("file-read.c"));
  _->chars = pwa_iterate(MemChars, ( ReadFdChunkChars(_->file) ));

  printf("hello ----\n");

  _->job = pwa_iterate(BackgroundJob, (1, 10, "spam"));
  pwa_async_job(_->job);

  _->job2 = pwa_iterate(BackgroundJob, (20, 0.3, "."));
  pwa_async_job(_->job2);

  pwa_for_s(_->c, _->chars) {
    printf("%c\n", _->c);
    if (_->c == '\n') break;
    pwa_delay(0.4);
  } pwa_end_for_s(_->chars)

  pwa_job_finish(_->job);
  pwa_sync_exec(_->job2);

  printf("fd %d\n", _->file.locals.fd);
} pwa_finally {
  pwa_catch(ReadFile) {
    printf("file error\n");
    printf("%s: %s\n", pwa_thrown_str, strerror(errno));
  }
  else pwa_catch_all {
    printf("unknown error\n");
  }
  printf("job main finishing...\n");
  pwa_delay(1);
  printf("job main finished!\n");
} pwa_end_func

char force = 0;

void onExit(int signum) {
  printf("exit signal!\n");
  if (force) {
    pwa_loop_all_jobs_kill(mainLoop);
    //pwa_loop_all_jobs_force_finish(mainLoop);
  } else {
    force = 1;
    pwa_loop_all_jobs_finish(mainLoop);
  }
}

int main(void) {
  //pwa_on_signals((SIGINT, SIGHUP, SIGTERM), onExit);
  pwa_on_exit_signals(onExit);

  pwa_iterate_var(main, Main, (0));

  pwa_loop_init(mainLoop);
  pwa_loop_async_job(mainLoop, main);
  pwa_loop_job_detach(mainLoop, main);
  // pwa_loop_async_job(mainLoop, main);
  pwa_loop_run(mainLoop);
  pwa_loop_free(mainLoop);

  printf("loop 2\n");

  pwa_loop_init(mainLoop);
  pwa_reset(main);
  pwa_loop_async_job(mainLoop, main);
  pwa_loop_run(mainLoop);
  pwa_loop_free(mainLoop);
  return 0;
}
