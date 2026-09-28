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
