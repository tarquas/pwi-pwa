// (c) 2022. Taras Mykhailovych. "Prywit Research Labs"
// `pw-iter`: PryWit - ITERators
// C99+ Language Header File
// Description:
//   A neat iterators utility using C macros. Generator functions with finalization/error handling support.
// How?
//   - Keeping shared execution context of generator function (arguments and local variables) in a `struct` (`locals`).
//   - Using scope-unleashed manner of `case` labels in a `switch` statement.
//   - Using counter (from `__COUNTER__` macro) for generating unique `case` labels.
//   - The code of generator function is organized in order, that each call to it for iterator's next value
//     is a separate call to this function. On each new call, `swicth` statement forwards execution to place after
//     previous iteration interruption statement.
// Bonus capabilities from architecture:
//   - Since the execution context is saved in a structure, the state of function call may be saved, restored, or tampered.
//     For saving the state, simply copy to another variable of the same iterator type using assignment operator `=`.
//     This should be used with caution, if function uses memory outside of `locals` (dynamic allocations etc.).
//   - Make closure by including parent functions' execution contexts into closure's.
// Caveats:
//   - As 'case' label in nested `switch` statements always means one for inner `switch`, using yield-statement
//     inside custom-defined `switch` statements is not supported. Using it will result in immediate exit from
//     generator function. Workarounds are:
//       - Using multiple `if`..`else` instead of switch.
//       - Using a callback map / nesting the generator function calls.
//   - The `locals` in code must be referred from related `struct` of iterator instance. 
//     The best way to do this (from author's viewpoint) is to use `_` variable as a pointer to `locals` and a
//     `_->` prefix before all shared parameters/local variables. This may look ugly, but author found it in a
//     philosophy of "curly arrow", meaning "from beyond", which reflects the intent of its usage.
//   - Minor performance drains for a logic around iteration interruption and unoptimized memory access
//     to `locals` from within a generator function code. For standard local scope, compiler may choose
//     to use CPU registers or better memory alignment.

#ifndef __PW_ITER__
#define __PW_ITER__

// open parentheses helper
#ifndef _pw_multi
#define _pw_multi(...) __VA_ARGS__
#endif

//

#define _pwi_state_final_bit ((long long)1 << 32)
#define _pwi_state_done_bit ((long long)1 << 40)

#define _pwi_state_stall ((long long) -_pwi_state_done_bit)

#define pwi_iterator(type, name) \
  typedef struct name * (*name ## _Func)(struct name *, void *); \
  typedef struct name { \
    unsigned long long state; \
    name ## _Func next; \
    void *error; \
    union { char done; void *tag; }; \
    _pw_multi type value; \
  } name

pwi_iterator((void *), pwi_Iterator);

static pwi_Iterator * _pwi_stall_next(pwi_Iterator *, void *);
static char _pwi_stall_error[] = "error: stall";

static pwi_Iterator _pwi_stall = {
  .state = (long long) -1,
  .next = _pwi_stall_next,
  .error = _pwi_stall_error,
  .done = -1,
  .value = 0,
};

static pwi_Iterator * _pwi_stall_next(pwi_Iterator *_, void *arg) {
  return &_pwi_stall;
}

// `case` labels for special iterator states (positive values are IDs of yield-statements)
#define _pwi_state_init 0
#define _pwi_state_final (-1)

// ** Define Iterator Type

// define iterator type and its generator function.
//   - name: Identifier -- name for new iterator type.
//   - type: (Type) -- a type of iterator value (i.e.: `(int)`).
//   - args: (StructFields) -- arguments of generator function in `struct` fields notation (i.e.: `(int a, b; float c)`).
//   - locals: (StructFields) -- local variables of generator function, which are used in several iterations.
//     (beyond any yield-statement, i.e.: `int i, j;` `float _;`).
//   - body: (Statements) -- a source code of generator function
//     (i.e.: `(for ($.i = 0; $.i < $.a; ++$.i) pwYield($.i);)`)
//   - finally: (Statements) -- a source code of generator function finalization (used to wrap-up the context,
//     useful when iterator is finished in the middle of execution.
#define pwi_func(type, name, args, vars) \
  typedef _pw_multi type name ## _type; \
  typedef struct name ## _Locals { \
    _pw_multi args; \
    _pw_multi vars; \
  } name ## _Locals; \
  typedef struct name { \
    unsigned long long state; \
    struct name * (*next)(struct name *, void *); \
    void *error; \
    union { char done; void *tag; }; \
    name ## _type value; \
    struct name ## _Locals locals; \
  } name; \
  name* name ## _func(name *_pwi_iter, void *arg) { \
    register unsigned long long _pwi_state = _pwi_iter->state; \
    if (_pwi_state & _pwi_state_stall) return (name *) &_pwi_stall; \
    register name ## _Locals *_ = &_pwi_iter->locals; \
    while (1) { \
      switch ((int) _pwi_state) { \
        case _pwi_state_init: \
        _pwi_iter->state = (unsigned)(int) _pwi_state_final;
  #define pwi_finally \
        case _pwi_state_final: \
        if (_pwi_state & _pwi_state_final_bit) break; \
        _pwi_iter->state = ((unsigned)(int) _pwi_state_final) | _pwi_state_final_bit;
  #define pwi_end_func \
        default: break; \
      } \
      _pwi_iter->state = ( \
        ((unsigned)(int) _pwi_state_final) | \
        _pwi_state_final_bit | _pwi_state_done_bit \
      ); \
      _pwi_iter->done = 1; \
      break; \
    } \
    return _pwi_iter; \
  }

#define pwi_iter_await_(_label) \
  *(unsigned *)&_pwi_iter->state = (unsigned)(_label); \
  return _pwi_iter; case (_label): \
  *(int *)&_pwi_iter->state = (int) _pwi_state_final;
#if defined(__COUNTER__)
  #define pwi_iter_await() pwi_iter_await_(__COUNTER__ + 1)
#elif defined(__LINE__)
  #define pwi_iter_await() pwi_iter_await_(__LINE__)
#else
  #error "compiler must have __COUNTER__ or __LINE__ capability"
#endif

// A yield-statement to pause generator function execution and return the indermediate value
#define pwi_yield(_value) { \
  _pwi_iter->value = (typeof(_pwi_iter->value))_value; \
  pwi_iter_await() \
}

#define pwi_exec(_iter) { \
  while (!((_iter).done)) { pwi_next(_iter); } \
}
#define pwi_finish_exec(_iter) { \
  while (!((_iter).done)) { pwi_finish(_iter); } \
}

// A yield-from-statement (`yield *` in JS) to output another iterator values
#define pwi_yields(_iter) { while (!(_iter).next(&(_iter), 0)->done) { pwi_yield((_iter).value); } }
#define pwi_throws(_iter) { if ((_iter).error) { pwi_throw((_iter).error); } }
#define pwi_returns(_iter) { pwi_throws(_iter); if ((_iter).done) { pwi_return((_iter).value); } }
#define pwi_exits(_iter) { pwi_throws(_iter); if ((_iter).done) { pwi_exit(); } }

#define pwi_next_s(_iter) { pwi_next(_iter); pwi_throws(_iter) }
#define pwi_finish_s(_iter) { pwi_finish(_iter); pwi_throws(_iter) }
#define pwi_fail_s(_iter) { pwi_fail(_iter); pwi_throws(_iter) }
#define pwi_halt_s(_iter) { pwi_halt(_iter); pwi_throws(_iter) }

#define pwi_exit() { _pwi_state = _pwi_iter->state; continue; }
#define pwi_shut() break

// A return-statement, which allows to execute the finalization prior to returning the final value
#define pwi_return(_value) { \
  _pwi_iter->error = 0; \
  _pwi_iter->value = (typeof(_pwi_iter->value)) _value; \
  pwi_exit() \
}

#define pwi_throw(_error) { \
  _pwi_iter->error = (void *) (_error); \
  pwi_exit() \
}

#define pwi_handled() { _pwi_iter->error = 0; pwi_exit() }
#define pwi_rethrow() if (_pwi_iter->error) pwi_exit()

// Iterator initialization
// ...
#define pwi_iterate(name, args) \
  (name) { \
    .state = _pwi_state_init, \
    .next = name ## _func, \
    .error = 0, .tag = 0, \
    .value = 0, \
    .locals = { _pw_multi args }, \
  }
#define pwi_iterate_var(id, name, args) name id = pwi_iterate(name, args)
#define pwi_reset(id) ((id).state = (int) _pwi_state_init)

#define pwi_next_(id, value) (id).next(&(id), (void *)(value))
#define pwi_next(id) pwi_next_(id, 0)

#define pwi_halt_(id, value) ( \
  *(int *)&_pwi_iter->state = (int) _pwi_state_final, \
  (id).next(&(id), (void *)(value)) \
)
#define pwi_halt(id) pwi_halt_(id, 0)

#define pwi_finish_(id, value) ( \
  (!((id).state & _pwi_state_final_bit)) && (*(int *)&(id).state = (int) _pwi_state_final), \
  (id).next(&(id), (void *)(value)) \
)
#define pwi_finish(id) pwi_finish_(id, 0)

#define pwi_kill(id) ( \
  (id).state = (unsigned)(int) _pwi_state_final | _pwi_state_final_bit | _pwi_state_done_bit \
)

#define pwi_fail_(id, _error, value) ( \
  *(int *)&(id).state = (int) _pwi_state_final, \
  ((id).error = (void *)(_error)), \
  (id).next(&(id), (void *)(value)) \
)
#define pwi_fail(id, _error) pwi_fail_(id, _error, 0)

#define pwi_for(_var, _iter) { \
  while (!pwi_next(_iter)->done) { \
    _var = (typeof((_iter).value)) (_iter).value;
#define pwi_end_for(_iter) \
  } pwi_finish_exec(_iter) \
}

#define pwi_for_s pwi_for
#define pwi_end_for_s(_iter) pwi_end_for(_iter) pwi_throws(_iter)

// errors

#define pwi_errors(type, ids, messages) \
  struct type ## _errorLayout { char _pw_multi ids; }; \
  char *type ## _errorMessages[] = { _pw_multi messages };
#define pwi_error(type, id) \
  (type ## _errorMessages + (size_t) &((struct type ## _errorLayout *)0)->id)
#define pwi_is_error(type, error) ( \
  (char *) error >= (char *) type ## _errorMessages && \
  (char *) error < (char *) type ## _errorMessages + sizeof(type ## _errorMessages) \
)
#define pwi_catch(type) if (pwi_is_error(type, _pwi_iter->error))
#define pwi_catch_all if (_pwi_iter->error)
#define pwi_error_str(error) (* (char **) error)
#define pwi_thrown (_pwi_iter->error)
#define pwi_thrown_str pwi_error_str(_pwi_iter->error)

#endif
