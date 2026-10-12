\ do/loop benchmark -- run with: time ./ash bench/loop.fs
\ 20M iterations of an empty-bodied counted loop plus one add: the
\ time is almost all loop bookkeeping, which Ash compiles as a thread.

: sum ( n -- sum ) 0 swap 0 do i + loop ;
: run 0 20 0 do drop 1000000 sum loop . cr ;
run
