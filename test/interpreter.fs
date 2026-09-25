\ kernel words exposed to forth
5 ' dup execute + .
: sq dup * ;
7 ' sq execute .
state @ .
>in @ .
\ counted strings poked little-endian (initial target is x86_64)
here 1886741507 over ! find swap drop .
here 15105 over ! find swap drop .
here 8026626 over ! find swap drop .
