\ naive recursion benchmark -- run with: time ./ash bench/fib.fs
\ fib(40) makes ~331M word calls; almost all time is threaded dispatch.

: fib dup 2 < if exit then dup 1- recurse swap 2 - recurse + ;
40 fib . cr
