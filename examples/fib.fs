\ fibonacci by naive recursion -- run with: ./ash examples/fib.fs

: fib ( n -- fib[n] )
  dup 2 < if exit then
  dup 1- recurse swap 2 - recurse + ;

: fibs 40 0 do i fib . loop cr ;

fibs
