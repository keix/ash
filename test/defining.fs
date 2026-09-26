\ create, does>, and their children
variable x
5 x !
x @ .
7 x ! x @ .
42 constant answer
answer .
cell .
' answer >body @ .
: adder create , does> @ + ;
5 adder add5
10 add5 .
: 2const create , , does> dup @ swap cell+ @ ;
3 4 2const pair
pair . .
\ stack must balance
depth .
