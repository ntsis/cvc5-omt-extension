; COMMAND-LINE:
; EXPECT: sat
(set-info :smt-lib-version 2.6)
(set-logic QF_SLIA)
(declare-const x0 String)
(declare-const x1 String)
(declare-const x2 String)
(declare-const x3 String)
(declare-const x4 String)
(declare-const x5 String)
(declare-const x6 String)
(declare-const x7 String)
(declare-const x8 String)
(declare-const x9 String)
(declare-const x10 String)
(declare-const y0 Int)
(declare-const y1 Int)
(declare-const y2 Int)
(declare-const y3 Int)
(declare-const y4 Int)
(declare-const y5 Int)
(declare-const y6 Int)
(declare-const y7 Int)
(declare-const y8 Int)
(declare-const y9 Int)
(declare-const y10 Int)
(assert (and (= y0 (str.to_code x0)) (>= y0 (str.to_code "0")) (<= y0 (str.to_code "9"))))
(assert (and (= y1 (str.to_code x1)) (>= y1 (str.to_code "0")) (<= y1 (str.to_code "9"))))
(assert (and (= y2 (str.to_code x2)) (>= y2 (str.to_code "0")) (<= y2 (str.to_code "9"))))
(assert (and (= y3 (str.to_code x3)) (>= y3 (str.to_code "0")) (<= y3 (str.to_code "9"))))
(assert (and (= y4 (str.to_code x4)) (>= y4 (str.to_code "0")) (<= y4 (str.to_code "9"))))
(assert (and (= y5 (str.to_code x5)) (>= y5 (str.to_code "0")) (<= y5 (str.to_code "9"))))
(assert (and (= y6 (str.to_code x6)) (>= y6 (str.to_code "0")) (<= y6 (str.to_code "9"))))
(assert (and (= y7 (str.to_code x7)) (>= y7 (str.to_code "0")) (<= y7 (str.to_code "9"))))
(assert (and (= y8 (str.to_code x8)) (>= y8 (str.to_code "0")) (<= y8 (str.to_code "9"))))
(assert (and (= y9 (str.to_code x9)) (>= y9 (str.to_code "0")) (<= y9 (str.to_code "9"))))
(assert (and (= y10 (str.to_code x10)) (>= y10 (str.to_code "0")) (<= y10 (str.to_code "9"))))
(assert (= (str.len x0) 1))
(assert (= (str.len x1) 1))
(assert (= (str.len x2) 1))
(assert (= (str.len x3) 1))
(assert (= (str.len x4) 1))
(assert (= (str.len x5) 1))
(assert (= (str.len x6) 1))
(assert (= (str.len x7) 1))
(assert (= (str.len x8) 1))
(assert (= (str.len x9) 1))
(assert (= (str.len x10) 1))
(check-sat)
