\ json.fs -- a small JSON pretty-printer: parse the text, re-emit it
\ indented. Integers only (ash has no floats); string escapes pass
\ through untouched. Malformed input throws -2 via abort", so a
\ parse is catchable like anything else.
\ run with: ./ash examples/json.fs

variable jpos
variable jend
variable depth

: jch ( -- c ) jpos @ c@ ;
: jnext 1 jpos +! ;
: jeof? jpos @ jend @ >= ;
: jskip begin jeof? 0= if jch 33 < else 0 then while jnext repeat ;
: jindent cr depth @ 2 * spaces ;
: jexpect ( c -- ) jskip jch = 0= abort" malformed JSON" jnext ;

\ the value parser recurses through arrays and objects before it can
\ be defined: a deferred word ties the knot at the end

defer jvalue

: jdigit? ( c -- f ) dup [char] 0 [char] : within swap [char] - = or ;
: jnumber begin jeof? 0= if jch jdigit? else 0 then
          while jch emit jnext repeat ;

\ strings pass through verbatim, a backslash carries its next char

: jstring
  [char] " emit jnext
  begin jeof? abort" malformed JSON" jch [char] " <> while
    jch [char] \ = if jch emit jnext then
    jch emit jnext
  repeat
  jnext [char] " emit ;

\ literals are verified as they are copied out

: jlit ( a u -- )
  begin dup 0> while
    over c@ jch <> abort" malformed JSON"
    over c@ emit jnext
    swap 1+ swap 1-
  repeat 2drop ;

: jarray
  [char] [ emit jnext 1 depth +!
  jskip jch [char] ] = if jnext -1 depth +! [char] ] emit exit then
  begin jindent jvalue jskip jch [char] , = while [char] , emit jnext
  repeat
  [char] ] jexpect -1 depth +! jindent [char] ] emit ;

: jpair
  jskip jch [char] " = 0= abort" malformed JSON"
  jstring [char] : jexpect ." : " jvalue ;

: jobject
  [char] { emit jnext 1 depth +!
  jskip jch [char] } = if jnext -1 depth +! [char] } emit exit then
  begin jindent jpair jskip jch [char] , = while [char] , emit jnext
  repeat
  [char] } jexpect -1 depth +! jindent [char] } emit ;

:noname
  jskip jeof? abort" malformed JSON"
  jch case
    [char] { of jobject endof
    [char] [ of jarray endof
    [char] " of jstring endof
    [char] t of s" true" jlit endof
    [char] f of s" false" jlit endof
    [char] n of s" null" jlit endof
    dup jdigit? 0= abort" malformed JSON" jnumber
  endcase ; is jvalue

: json ( a u -- ) over + jend ! jpos ! 0 depth ! jvalue jskip cr ;

\ ---- demo ----

: demo
  s\" {\"name\": \"ash\", \"cells\": 64, \"jit\": true, \"tags\": [\"forth\", \"ans\", {\"nested\": null}], \"empty\": [], \"neg\": -42}"
  json ;

demo

\ a malformed document throws; catch it like anything else. The
\ printer is streaming, so output stops exactly where parsing did.

: bad s\" {\"oops\": }" json ;
cr .( a malformed document prints until it throws:) cr
' bad catch
cr .( caught: ) . cr
