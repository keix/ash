\ programming-tools extensions
: pt1 ahead 1111 2222 then 3333 ;
t{ pt1 -> 3333 }t
t{ true [if] 111 [else] 222 [then] -> 111 }t
t{ false [if] 111 [else] 222 [then] -> 222 }t
t{ [defined] pt1 -> -1 }t
t{ [defined] no-such-word-x -> 0 }t
t{ [undefined] pt1 -> 0 }t
t{ [undefined] no-such-word-x -> -1 }t
: ntr n>r -1 nr> ;
t{ 1 2 3 4 5 6 7 4 ntr -> 1 2 3 -1 4 5 6 7 4 }t
t{ 1 0 ntr -> 1 -1 0 }t
: syn-src 1234 ;
synonym syn-new syn-src
t{ syn-new -> 1234 }t
: syn-imm 2345 ; immediate
synonym syn-inew syn-imm
t{ : syn-use syn-inew literal ; syn-use -> 2345 }t
: ?done postpone if 1 cs-roll ; immediate
: cw begin dup 5 < ?done 1+ repeat ;
t{ 0 cw -> 5 }t
