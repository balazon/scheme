# scheme
A scheme interpreter written in C++

To compile this, you need C++11 compiler settings (just set -std=c++11 in compiler options):

  Get in the folder containing main.cpp, scheme.h and scheme.cpp, and there:
  g++ -g -O0 -Wall -std=c++11 scheme.cpp main.cpp -o scheme

Run the binary from console to run the interpreter, there you can run scheme commands:

  (+ 2 3)   ; 5
  
  (define x 4)  ;set variable x to 4
  
  (define myfunc (lambda (y) (* x y)))  ;defining a function - first method
  
  (define (myfunc2 y) (/ x y))  ;defining a function - second method
  
  (myfunc 6)   ;calling function - will output 24
  
  (myfunc2 2) ; calling function - will output 2
  
  (define (fact n) (if (= n 0) 1 (* n (fact (- n 1))))) ; factorial function
  
  (fact 6)  ; calling factorial - outputs 720
  
  (define (adder z) (lambda (x) (+ z x))) ;a function returning a function
  
  (define plus2 (adder 2))  ;plus2 is a function that adds 2 to its argument
  
  (plus2 3) ;5
  
  help  ;the help command prints the help dialog
  
  exit  ;quits the interpreter
