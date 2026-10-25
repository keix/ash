\ double-number word set
t{ 1. -> 1 0 }t
t{ -1. -> -1 -1 }t
t{ 0. -> 0 0 }t
t{ 1. 2. d+ -> 3 0 }t
t{ -1. -2. d+ -> -3 -1 }t
t{ 10. 4. d- -> 6 0 }t
t{ 1. 1. d= -> -1 }t
t{ 1. 2. d= -> 0 }t
t{ 1. 2. d< -> -1 }t
t{ 2. 1. d< -> 0 }t
t{ 0. d0= -> -1 }t
t{ 1. d0= -> 0 }t
t{ -1. d0< -> -1 }t
t{ 1. d0< -> 0 }t
t{ 1. 2. dmax -> 2 0 }t
t{ 1. 2. dmin -> 1 0 }t
t{ 3. d2* -> 6 0 }t
t{ 6. d2/ -> 3 0 }t
t{ -1. d2/ -> -1 -1 }t
t{ -5. dnegate -> 5 0 }t
t{ -5. dabs -> 5 0 }t
t{ 1234 0 d>s -> 1234 }t
t{ 1 0 2 0 du< -> -1 }t
t{ 2 0 1 0 du< -> 0 }t
t{ 5 7 m* -> 35 0 }t
t{ -5 7 m* -> -35 -1 }t
t{ 5. 7 11 m*/ -> 3 0 }t
t{ 5. 3 m+ -> 8 0 }t
2variable dv
t{ 3. dv 2! dv 2@ -> 3 0 }t
10. 2constant tc
t{ tc -> 10 0 }t
1111 2222 2value 2v
t{ 2v -> 1111 2222 }t
t{ 3333 4444 to 2v 2v -> 3333 4444 }t
: dq [ 7. ] 2literal ;
t{ dq -> 7 0 }t
