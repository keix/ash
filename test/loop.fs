\ loops written in forth
: countup 0 begin dup 5 < while dup . 1+ repeat drop ;
countup
: idx 5 0 do i . loop ;
idx
: grid 2 0 do 3 0 do i . loop loop ;
grid
: early 10 0 do i . i 2 = if unloop exit then loop ;
early
: sum 0 swap 1+ 1 do i + loop ;
10 sum .
\ stack must balance
depth .
