\ byte sieve benchmark -- run with: time ./ash bench/sieve.fs
\ the classic 8190-flag sieve, 1000 passes: c@ c! and nested loops.

8190 constant size
create flags size allot

: sieve ( -- primes )
  flags size 1 fill
  0 size 0 do
    flags i + c@ if
      i dup + 3 + dup i +
      begin dup size < while 0 over flags + c! over + repeat
      2drop 1+
    then
  loop ;

: run 0 1000 0 do drop sieve loop . cr ;
run
