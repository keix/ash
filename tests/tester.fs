\ tester.fs -- a minimal Hayes-style test harness.
\ Standard words only, so the same tests run under gforth as an
\ oracle:  t{ <code> -> <expected> }t

variable start-depth
variable actual-depth
create actual-results 32 cells allot
variable tests
variable errors
0 tests !
0 errors !

\ report and drain back to the baseline so one failure cannot
\ poison the next test
: error
  cr type cr
  errors @ 1+ errors !
  begin depth start-depth @ > while drop repeat ;

: t{ tests @ 1+ tests ! depth start-depth ! ;

\ record the actual results, bottom to top, and clear them
: ->
  depth actual-depth !
  begin depth start-depth @ > while
    depth start-depth @ - 1- cells actual-results + !
  repeat ;

\ compare the expected results against the recorded ones
: }t
  depth actual-depth @ <> if
    s" }t: wrong number of results" error
  else
    begin depth start-depth @ > while
      depth start-depth @ - 1- cells actual-results + @
      <> if s" }t: incorrect result" error then
    repeat
  then ;

: test-summary
  errors @ 0= if
    tests @ . ." tests: all forth tests passed" cr
  else
    errors @ . ." of " tests @ . ." tests failed" cr
  then ;
