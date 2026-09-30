\ source, word, environment?
t{ source nip 0> -> -1 }t
t{ bl word hello count nip -> 5 }t
t{ bl word xyz count drop c@ -> 120 }t
t{ 44 word abc, count nip -> 3 }t
t{ 44 word ,,abc, count nip -> 3 }t
t{ 44 word abc, count drop c@ -> 97 }t
: eq s" no-such-query" environment? ;
t{ eq -> 0 }t

\ evaluate: the text interpreter in forth
: ev1 s" 1 2 +" evaluate ;
t{ ev1 -> 3 }t
: ev2 s" 10 3 mod" evaluate ;
t{ ev2 -> 1 }t
: ev7 s" -42" evaluate ;
t{ ev7 -> -42 }t
: inner-src s" 7 8 +" evaluate ;
: ev4 s" inner-src 2 *" evaluate ;
t{ ev4 -> 30 }t
: ev5 s" : sq9 9 dup * ;" evaluate ;
ev5
t{ sq9 -> 81 }t
: evt s" 55 throw" evaluate ;
t{ ' evt catch -> 55 }t
: evu s" nonsense-word" evaluate ;
t{ ' evu catch -> -13 }t
: evn s" 4x5" evaluate ;
t{ ' evn catch -> -13 }t
