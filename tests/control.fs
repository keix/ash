\ control flow
: cf1 if 111 else 222 then ;
t{ -1 cf1 -> 111 }t
t{ 0 cf1 -> 222 }t
: cf2 0 begin 1+ dup 5 = until ;
t{ cf2 -> 5 }t
: cf3 0 begin dup 5 < while 1+ repeat ;
t{ cf3 -> 5 }t
: cf4 0 5 0 do i + loop ;
t{ cf4 -> 10 }t
: cf5 10 0 do i i 3 = if unloop exit then loop ;
t{ cf5 -> 0 1 2 3 }t
: cf6 0 2 0 do 3 0 do 1+ loop loop ;
t{ cf6 -> 6 }t
