(define (mod x y)
  (- x (* (/ x y) y)))

(define (divisor n d)
  (if (< n (* d d))
      0
      (if (= (mod n d) 0)
          1
          (divisor n (+ d 1)))))

(define (prime n)
  (if (< n 2)
      0
      (if (= (divisor n 2) 1)
          0
          1)))

(define (next n)
  (if (= (prime (+ n 1)) 1)
      (+ n 1)
      (next (+ n 1))))

(define (nth k)
  (if (= k 1)
      2
      (next (nth (- k 1)))))

(nth 10000) ;; 104729