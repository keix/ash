\ primes below 50 by trial division -- run with: ./ash examples/primes.fs

: prime? ( n -- flag )
  dup 2 < if drop 0 exit then
  dup 2 = if drop -1 exit then
  dup 2 do
    dup i mod 0= if drop 0 unloop exit then
  loop drop -1 ;

: primes 50 2 do i prime? if i . then loop cr ;

primes
