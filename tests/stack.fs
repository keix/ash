\ stack words
t{ 1 2 swap -> 2 1 }t
t{ 1 dup -> 1 1 }t
t{ 1 2 drop -> 1 }t
t{ 1 2 over -> 1 2 1 }t
t{ 1 2 3 rot -> 2 3 1 }t
t{ 7 3 nip -> 3 }t
t{ 1 2 tuck -> 2 1 2 }t
t{ 4 5 2dup -> 4 5 4 5 }t
t{ 4 5 2drop -> }t
t{ 5 ?dup -> 5 5 }t
t{ 0 ?dup -> 0 }t
t{ depth -> 0 }t
t{ 1 2 depth -> 1 2 2 }t
t{ 5 >r r@ r> -> 5 5 }t
t{ 1 2 >r >r r> r> -> 1 2 }t
