\ unbounded recursion: the return stack's guard page stops it, after
\ the jit has burned the word
1 2 + . cr
: deeper recurse ;
deeper
