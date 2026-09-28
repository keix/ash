\ the compiler surface: ticks, execute, find, defining words
t{ ' dup ' dup = -> -1 }t
: sq dup * ;
t{ 7 ' sq execute -> 49 }t
t{ state @ -> 0 }t
t{ >in @ 0> -> -1 }t
t{ char A -> 65 }t
: bq [char] Q ;
t{ bq -> 81 }t

\ find takes a counted string; build them portably with c,
create fname 3 c, char d c, char u c, char p c,
create sname 1 c, char ; c,
create nname 2 c, char z c, char z c,
t{ fname find nip -> -1 }t
t{ fname find drop ' dup = -> -1 }t
t{ sname find nip -> 1 }t
t{ nname find nip -> 0 }t
t{ nname find drop nname = -> -1 }t

\ create / does> and friends
variable tv
t{ 5 tv ! tv @ -> 5 }t
42 constant ans
t{ ans -> 42 }t
t{ ' ans >body @ -> 42 }t
: adder create , does> @ + ;
5 adder a5
t{ 10 a5 -> 15 }t

\ string words leave addr len; check the parts
: sx s" ab" ;
t{ sx nip -> 2 }t
t{ sx drop c@ -> 97 }t
t{ sx drop 1+ c@ -> 98 }t
