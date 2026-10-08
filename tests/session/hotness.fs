\ hot words burn by default -- by call count, and by loop iteration
: fresh ;
\ a loop in a word called once: it burns on its own backedge, mid-call
: sum 0 swap 0 do i + loop ;
1000000 sum .
' sum @ ' fresh @ <> .
\ begin/until loops ride 0branch backward
: count-up 0 begin 1+ dup 100000 < 0= until ;
count-up .
' count-up @ ' fresh @ <> .
\ a headerless word is found by its shape
:noname 0 swap 0 do i + loop ;
dup 1000 swap execute .
@ ' fresh @ <> .
\ a loop after does> stays threaded and keeps working
: ntimes create , does> @ 0 swap 0 do i + loop ;
5000 ntimes big
big .
' ntimes @ ' fresh @ = .
\ nested: the inner loop burns the whole word, the outer continues in native code
: nest 0 200 0 do 100 0 do 1+ loop loop ;
nest .
depth .
