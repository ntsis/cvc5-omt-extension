; DISABLE-TESTER: dump
; REQUIRES: no-competition
; SCRUBBER: grep -o "All formal arguments to defined functions must be unique"
; EXPECT: All formal arguments to defined functions must be unique
; EXIT: 1
(set-logic ALL)
(declare-const x2 Bool)
(define-fun s ((a Int) (b Int) (z Int) (z Int)) Bool (= 0 0))
(define-fun p ((a Int) (b Int) (c Int)) Bool (exists ((x Int) (y Int)) (and false (s 0 0 b c) (forall ((z Int)) false))))
(assert (p 0 0 0))
(check-sat)
