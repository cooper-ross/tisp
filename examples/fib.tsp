(define (fib n)
  (cond
    [(< n 2) n]
    [1 (+ (fib (- n 1)) (fib (- n 2)))]))

(fib 20) ;; 6765