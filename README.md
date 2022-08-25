# Yabil
Yet Another Byte-code Interpreted Language written in C. Yabil is a Lox clone followed from the book 'Crafting Interpreters' by Robert Nystrom with a few extra features. Aside from dynamic typing, classes, higher order functions and closures, basic control flow and string interning, Yabil also supports the following features:
- Dynamic array objects
- Array indexing to get and set
- Array concatenation using the '+' operator
  - `[1, 2, 3] + [4, 5]` results in `[1, 2, 3, 4, 5]`
  - `[1, 2, 3] + arbitrary_val` results in `[1, 2, 3, arbitrary_val]`
- String indexing to get and set
- Javascript style object field access through string, i.e., `obj["field"]`
- Ternary operator

Furthermore, Yabil scripts are parsed using a Pratt top-down parser and compiled to a byte-code representation. The Virtual Machine executes the byte-code by which the internal stack gets manipulated.   

## Quick Start
```
$ make
$ ./yabil <filename>
```
Provide filename to execute script or run without filename to run the REPL (Run Execute Print Loop) environment 
