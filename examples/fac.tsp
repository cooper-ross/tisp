(define (factorial n)
  (cond
   [(= 0 n) 1]
   [1 (* n (factorial (- n 1)))]))

(factorial 5) ;; 120