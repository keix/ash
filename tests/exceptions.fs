\ catch and throw
: thrower 42 throw ;
: nothrow 7 ;
t{ 5 ' thrower catch -> 5 42 }t
t{ 1 2 ' nothrow catch -> 1 2 7 0 }t
: mid ['] thrower catch 100 + ;
t{ ' mid catch -> 142 0 }t
: messy 111 222 333 9 throw ;
t{ ' messy catch -> 9 }t
t{ 5 0 throw -> 5 }t
t{ ' abort catch -> -1 }t
: ab-bad 1 abort" boom" 7 ;
t{ ' ab-bad catch -> -2 }t
: ab-ok 0 abort" boom" 7 ;
t{ ' ab-ok catch -> 7 0 }t
\ handler unwinds correctly when nested catches both survive
: inner ['] thrower catch drop 8 ;
: outer ['] inner catch ;
t{ outer -> 8 0 }t
