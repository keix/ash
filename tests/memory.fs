\ memory and data space
variable mv
t{ 42 mv ! mv @ -> 42 }t
t{ 7 mv ! mv @ -> 7 }t
t{ here 1 cells allot here swap - -> 1 cells }t
t{ here 1 allot here swap - -> 1 }t
align
t{ here 42 over ! @ -> 42 }t
t{ here 65 over c! c@ -> 65 }t
t{ here 65 c, c@ -> 65 }t
align
t{ here 2 c, 65 c, 66 c, count nip -> 2 }t
align
t{ here 2 c, 65 c, 66 c, count drop c@ -> 65 }t
align
t{ 0 char+ -> 1 }t
t{ 5 chars -> 5 }t
t{ 0 aligned -> 0 }t
t{ 1 aligned -> 1 cells }t
t{ here aligned cell mod -> 0 }t
variable pv
t{ 5 pv ! 3 pv +! pv @ -> 8 }t
create dv 2 cells allot
t{ 3 4 dv 2! dv 2@ -> 3 4 }t
t{ here 3 65 fill here c@ here 1+ c@ here 2 + c@ -> 65 65 65 }t
t{ here 0 66 fill here c@ -> 65 }t
create mbuf 16 allot
: mset 65 mbuf c! 66 mbuf 1+ c! 67 mbuf 2 + c! ;
t{ mset mbuf mbuf 8 + 3 move mbuf 8 + c@ mbuf 9 + c@ mbuf 10 + c@ -> 65 66 67 }t
t{ mset mbuf mbuf 1+ 3 move mbuf c@ mbuf 1+ c@ mbuf 2 + c@ mbuf 3 + c@ -> 65 65 66 67 }t
t{ mset mbuf 1+ mbuf 2 move mbuf c@ mbuf 1+ c@ -> 66 67 }t
t{ mbuf mbuf 0 move -> }t
