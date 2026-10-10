\ core extension words
t{ 1 2 u> -> 0 }t
t{ 2 1 u> -> -1 }t
t{ -1 1 u> -> -1 }t
t{ 1 2 3 2 pick -> 1 2 3 1 }t
t{ 1 2 3 0 pick -> 1 2 3 3 }t
t{ 1 2 3 2 roll -> 2 3 1 }t
t{ 1 2 3 0 roll -> 1 2 3 }t
: tr2 2>r 2r@ 2r> ;
t{ 1 2 tr2 -> 1 2 1 2 }t
: cs1 case 1 of 111 endof 2 of 222 endof 333 swap endcase ;
t{ 1 cs1 -> 111 }t
t{ 2 cs1 -> 222 }t
t{ 9 cs1 -> 333 }t
5 value vt
t{ vt -> 5 }t
t{ 7 to vt vt -> 7 }t
: vset 9 to vt ;
t{ vset vt -> 9 }t
defer df
' * is df
t{ 3 4 df -> 12 }t
t{ ' df defer@ ' * = -> -1 }t
' + ' df defer! 
t{ 3 4 df -> 7 }t
t{ action-of df ' + = -> -1 }t
16 buffer: bb
t{ 3 bb c! bb c@ -> 3 }t
: cc c" hey" ;
t{ cc c@ -> 3 }t
t{ cc count nip -> 3 }t
t{ cc count drop c@ -> 104 }t
: sx1 s\" a\nb" ;
t{ sx1 nip -> 3 }t
t{ sx1 drop 1+ c@ -> 10 }t
t{ pad 4 65 fill pad 4 erase pad c@ pad 3 + c@ -> 0 0 }t
t{ 5 :noname [ ' dup compile, ' * compile, ] ; execute -> 25 }t
t{ unused 0> -> -1 }t
t{ source-id 0<> -> -1 }t
: rf s" refill" evaluate ;
t{ rf -> 0 }t
: sri save-input restore-input ;
t{ sri -> 0 }t
t{ true [if] 111 [else] 222 [then] -> 111 }t
t{ false [if] 111 [else] 222 [then] -> 222 }t
t{ false [if] 1 [then] 42 -> 42 }t
t{ true [if] 1 false [if] 2 [then] 3 [then] -> 1 3 }t
t{ false [if] true [if] 2 [then] [else] 9 [then] -> 9 }t
