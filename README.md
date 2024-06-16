# Kokos
Kokos is an interpreted lisp dialect designed for simple scripting.

# Features

## Language level

- [X] Arithmetic operators
- [X] Floats
- [ ] UTF-8 strings
- [X] Variables
- [X] Functions
- [X] Macros
- [X] Scoped variable bindings
- [X] Basic equality and comparison operators
- [X] Dynamic arrays
- [X] Maps
- [X] Lambdas
- [ ] Lazy evaluation
- [X] File I/O
- [ ] Module system
- [ ] Standard library

## Implementation (software) level

- [X] Repl
- [X] Garbage collector
- [X] Fully working interpreter
- [X] Virtual machine
- [X] C embedding
- [ ] C FFI
- [ ] Tail call optimization

# Building from source
Currently it is the only option to get kokos, but i plan to provide binary releases in the future.

```console
$ meson setup build
$ cd build
$ meson compile
```

# Overview

### The basics

```lisp
(+ 1 2 3) ; => 6
(* 4 1.7) ; => 6.8

(if (> 3 1) (print "greater!") (print "not greater"))

(make-vec 1 0 3) ; => [1 0 3]
(make-map "hello" "world") ; => {"hello" "world"}

; or just use collection literals:
; [1 0 3]
; {"hello" "world"}
```

### Procedures

```lisp
(proc add (x y) (+ x y))

(add 1 2) ; => 3

(proc variadic (& rest) (print rest))
(variadic 1 2 3 "some string") ; => [1 2 3 "some string"]
```

### Macros
Kokos currently supports macros, however the reader macros are not yet implemented. The macros will be documented once the reader macros are implemented.

# Notes
- The vm is currenly about 5 times faster than the interpreter, but does not have a lot of features and procedures implemented yet
