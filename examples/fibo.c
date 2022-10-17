#include <stdio.h>
#include "pw-iter.h"

pwi_errors(FibonacciNumbers, ( // define error type for iterator type FibonacciNumbers
  badLimit, unknown // define error IDs, specific to iterators of this type; comma-separated
), (
  "bad argument: limit", "unknown" // define error messages in the same order as error IDs
))

// define generator function for iterator type FibonacciNumbers:
//     (type), IteratorTypeName, (arguments), (local variables)
pwi_func((int), FibonacciNumbers, (int limit), ( // iteration results are `int` numbers; 1 input argument: `limit`
  int pprev, prev, cur, n; // local variables; these are used beyond yield/throw/return-statements
)) { // curly braces are optional: they are used to prettify and isolate the main scope from final
  if (_->limit < 0) { // all `locals` (iterator-scope arguments and local variables) are prefixed with `_->`
    pwi_throw(pwi_error(FibonacciNumbers, badLimit)); // throw specific error (defined with `pwi_errors` above)
  }

  _->prev = _->cur = 1;
  pwi_yield(0); // yield statement. emit value `0`
  _->n = 1;

  while (_->cur < _->limit) {
    _->pprev = _->prev;
    _->prev = _->cur;
    pwi_yield(_->cur); // yield each next value
    ++ _->n;
    _->cur += _->pprev;
  }
} pwi_finally { // the following code will be executed before exit from iteration, unless forced exit is used
  pwi_rethrow(); // do not continue execution on error
  pwi_return(_->n); // return number of emitted values on success
} pwi_end_func // ends the code of `pwi_func`

pwi_func((int), Main, (), ()) { // no arguments and iterator-scoped local variables are used. dummy `int` iterator type
  pwi_iterate_var(fib, FibonacciNumbers, (100)); // create `fib` iterator with `limit = 100` argument

  pwi_next_s(fib); // get next value from `fib`
  //pwi_throws(fib); // if error is thrown in `fib`, do not continue to next line; throw it from this iterator
  printf("first: %d\n", fib.value);

  pwi_for_s(int n, fib) {
    printf("next: %d\n", n);
  } pwi_end_for_s(fib);

  printf("%d numbers generated\n", fib.value);
pwi_finally
  pwi_catch(FibonacciNumbers) {
    printf("FibonacciNumbers error: %s\n", pwi_thrown_str);
  }
  else pwi_catch_all {
    printf("Unhandled error: %s\n", pwi_error_str(fib.error));
  }
} pwi_end_func

int main(void) {
  pwi_iterate_var(main, Main, ());
  pwi_exec(main);
  return main.error ? 1 : 0;
}
