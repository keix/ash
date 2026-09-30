\ source, word, environment?
t{ source nip 0> -> -1 }t
t{ bl word hello count nip -> 5 }t
t{ bl word xyz count drop c@ -> 120 }t
t{ 44 word abc, count nip -> 3 }t
t{ 44 word ,,abc, count nip -> 3 }t
t{ 44 word abc, count drop c@ -> 97 }t
: eq s" no-such-query" environment? ;
t{ eq -> 0 }t
