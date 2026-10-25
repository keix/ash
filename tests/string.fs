\ string word set (Forth-94 base)
: s1 s" abcdefghij" ;
: s2 s" abc" ;
: s3 s" cde" ;
t{ s1 0 /string -> s1 }t
t{ s1 3 /string nip -> 7 }t
t{ s1 3 /string drop c@ -> 100 }t
t{ s" abc  " -trailing nip -> 3 }t
t{ s" abc" -trailing nip -> 3 }t
t{ s" " -trailing nip -> 0 }t
t{ s2 s2 compare -> 0 }t
t{ s2 s" abd" compare -> -1 }t
t{ s" abd" s2 compare -> 1 }t
t{ s2 s" ab" compare -> 1 }t
t{ s" ab" s2 compare -> -1 }t
t{ s1 s2 search nip nip -> -1 }t
t{ s1 s2 search 2drop c@ -> 97 }t
t{ s1 s3 search nip nip -> -1 }t
t{ s1 s3 search 2drop c@ -> 99 }t
t{ s1 s3 search drop nip -> 8 }t
t{ s1 s" xyz" search nip nip -> 0 }t
t{ s1 s" " search nip nip -> -1 }t
t{ pad 5 blank pad c@ pad 4 + c@ -> 32 32 }t
t{ pad 3 66 fill pad c@ -> 66 }t
: sl [ s" hi" ] sliteral ;
t{ sl nip -> 2 }t
t{ sl 2drop -> }t
t{ sl drop c@ -> 104 }t
create cb1 10 allot
t{ s2 cb1 swap cmove cb1 3 s2 compare -> 0 }t
