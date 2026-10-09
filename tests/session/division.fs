\ division by zero throws -10, an ash guarantee beyond the standard
: dz 5 0 / ;
' dz catch . cr
: dzm 5 0 mod ;
' dzm catch . cr
: dzf 5 s>d 0 fm/mod ;
' dzf catch . cr
\ stack must balance
depth .
