#include <stdio.h>
#include "pw-async.h"

pwi_func((int), Range, (int start, end, step), (
  int i;
)) {
  if (!_->step) { _->step = 1; }
  if (_->step > 0) {
    for (_->i = _->start; _->i < _->end; ++_->i) {
      pwi_yield(_->i);
    }
  } else {
    for (_->i = _->start; _->i > _->end; --_->i) {
      pwi_yield(_->i);
    }
  }
} pwi_end_func

pwi_iterator((int), IntIterator);

pwi_func((int), Add, (IntIterator *iter; int delta), (
  int i;
)) {
  pwi_for_s(_->i, *_->iter) {
    pwi_yield(_->i + _->delta);
  } pwi_end_for_s(*_->iter)
} pwi_end_func

int main(void) {
  struct timespec start, end;
  pwa_timespec_monoClockIn(&start, 0.0);
  Add *a;

  pwi_iterate_var(range, Range, (0, 1e8)); // 1.7s
  pwi_iterate_var(add1, Add, ((IntIterator *) &range, 1)); a = &add1; // 1.8 s // 1.5
  /*
  pwi_iterate_var(add2, Add, ((IntIterator *) &add1, 1)); a = &add2; // 2.4 s
  pwi_iterate_var(add3, Add, ((IntIterator *) &add2, 1)); a = &add3; // 3.1 s
  pwi_iterate_var(add4, Add, ((IntIterator *) &add3, 1)); a = &add4; // 3.6 s
  pwi_iterate_var(add5, Add, ((IntIterator *) &add4, 1)); a = &add5; // 4.5 s
  pwi_iterate_var(add6, Add, ((IntIterator *) &add5, 1)); a = &add6; // 5.0 s
  pwi_iterate_var(add7, Add, ((IntIterator *) &add6, 1)); a = &add7; // 5.3 s
  pwi_iterate_var(add8, Add, ((IntIterator *) &add7, 1)); a = &add8; // 6.2 s
  pwi_iterate_var(add9, Add, ((IntIterator *) &add8, 1)); a = &add9; // 6.8 s
  pwi_iterate_var(add10, Add, ((IntIterator *) &add9, 1)); a = &add10; // 9.4 s
  pwi_iterate_var(add11, Add, ((IntIterator *) &add10, 1)); a = &add11; // 9.2 s
  pwi_iterate_var(add12, Add, ((IntIterator *) &add11, 1)); a = &add12; // 9.5 s
  pwi_iterate_var(add13, Add, ((IntIterator *) &add12, 1)); a = &add13; // 10.3 s
  pwi_iterate_var(add14, Add, ((IntIterator *) &add13, 1)); a = &add14; // 11.3 s // 9.3s
  */
  pwi_iterate_var(add, Add, ((IntIterator *) a, 1));

  //int sum = 0; for (int i = 0; i < 1e8; ++i) sum += i + 1 + 1;
  //pwi_exec(add);
  //pwi_exec(range);ß

  //*
  int sum = 0; pwi_for(int i, add) {
    sum += i;
  } pwi_end_for(add)
  // */

  pwa_timespec_monoClockIn(&end, 0.0);
  printf("sum: %d\n", sum);
  printf("time: %lf\n", pwa_timespec_diff_sec(&end, &start));
  return 0;
}
