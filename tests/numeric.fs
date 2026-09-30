\ pictured numeric output
t{ base @ -> 10 }t
t{ 1234 0 <# #s #> nip -> 4 }t
t{ 1234 0 <# #s #> drop c@ -> 49 }t
t{ 1234 0 <# #s #> drop 3 + c@ -> 52 }t
t{ 0 0 <# #s #> nip -> 1 }t
t{ 0 0 <# #s #> drop c@ -> 48 }t
t{ 255 0 <# # # #> nip -> 2 }t
t{ 255 0 <# # # #> drop c@ -> 53 }t
\ push the number first: after base ! the literal 255 would itself
\ be parsed as hex
t{ 255 0 16 base ! <# #s #> nip decimal -> 2 }t
t{ 255 0 16 base ! <# #s #> drop c@ decimal -> 70 }t
t{ -123 dup abs 0 <# #s rot sign #> nip -> 4 }t
t{ -123 dup abs 0 <# #s rot sign #> drop c@ -> 45 }t
: nstr1 s" 123" ;
t{ 0 0 nstr1 >number 2drop -> 123 0 }t
: nstr2 s" 45x7" ;
t{ 0 0 nstr2 >number nip nip nip -> 2 }t
t{ 0 0 nstr2 >number 2drop drop -> 45 }t
t{ 0 0 nstr2 >number drop nstr2 drop - nip nip -> 2 }t
: nstr4 s" 18446744073709551616" ;
t{ 0 0 nstr4 >number 2drop -> 0 1 }t
