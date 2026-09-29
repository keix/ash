\ arithmetic and comparison
t{ 1 2 + -> 3 }t
t{ 10 4 - -> 6 }t
t{ 6 7 * -> 42 }t
t{ 22 7 / -> 3 }t
t{ 22 7 mod -> 1 }t
t{ -8 negate -> 8 }t
t{ 8 negate -> -8 }t
t{ -7 abs -> 7 }t
t{ 3 abs -> 3 }t
t{ 3 9 min -> 3 }t
t{ 9 3 min -> 3 }t
t{ 3 9 max -> 9 }t
t{ 9 3 max -> 9 }t
t{ 10 1+ -> 11 }t
t{ 10 1- -> 9 }t
t{ 10 2* -> 20 }t
t{ 10 2/ -> 5 }t
t{ 3 5 < -> -1 }t
t{ 5 3 < -> 0 }t
t{ 5 3 > -> -1 }t
t{ 3 5 > -> 0 }t
t{ 4 4 = -> -1 }t
t{ 4 5 = -> 0 }t
t{ 0 0= -> -1 }t
t{ 1 0= -> 0 }t
t{ 3 4 <> -> -1 }t
t{ 4 4 <> -> 0 }t
t{ -3 0< -> -1 }t
t{ 3 0< -> 0 }t
t{ 3 0> -> -1 }t
t{ -3 0> -> 0 }t
t{ 12 10 and -> 8 }t
t{ 12 10 or -> 14 }t
t{ 12 10 xor -> 6 }t
t{ 0 invert -> -1 }t
t{ -1 invert -> 0 }t
t{ 1 4 lshift -> 16 }t
t{ 16 2 rshift -> 4 }t
t{ 1 0 lshift -> 1 }t
t{ 22 7 /mod -> 1 3 }t
t{ 1 2 u< -> -1 }t
t{ 2 1 u< -> 0 }t
t{ -1 1 u< -> 0 }t
t{ 1 -1 u< -> -1 }t
t{ 0 0 u< -> 0 }t
t{ 5 s>d -> 5 0 }t
t{ -5 s>d -> -5 -1 }t
