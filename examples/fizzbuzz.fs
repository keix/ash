\ fizzbuzz -- run with: ./ash examples/fizzbuzz.fs

: fizzbuzz
  101 1 do
    i 15 mod 0= if ." FizzBuzz" cr else
    i 3 mod 0= if ." Fizz" cr else
    i 5 mod 0= if ." Buzz" cr else
    i . cr
    then then then
  loop ;

fizzbuzz
