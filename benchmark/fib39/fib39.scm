(define fib
  (lambda (n)
    (if (< n 2)
        n
        (+ (fib (- n 2)) (fib (- n 1))))))

(print (fib 39))
