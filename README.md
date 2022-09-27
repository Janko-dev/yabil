# Yabil
Yet Another Byte-code Interpreted Language written in C. Yabil is a Lox clone followed from the book `Crafting Interpreters` by Robert Nystrom with a few extra features. Aside from dynamic typing, classes, higher order functions and closures, basic control flow and string interning, Yabil also supports the following features:
- Dynamic array objects
- Array indexing to get and set
- Array concatenation using the '+' operator
  - `[1, 2, 3] + [4, 5]` results in `[1, 2, 3, 4, 5]`
  - `[1, 2, 3] + arbitrary_val` results in `[1, 2, 3, arbitrary_val]`
- String indexing to get and set
- Javascript style object field access through string, i.e., `obj["field"]`
- Ternary operator

Furthermore, Yabil scripts are parsed using a Pratt top-down parser and compiled to a byte-code representation. The Virtual Machine executes the byte-code by which the internal stack gets manipulated. This stack-based approach is very easy to implement and understand. 

## Interesting concepts
An interesting notion of this C application is the concept of NaN-boxing, where the advantage of NaNs (Not a Number) of the IEEE 754 floating point standard is used to encode more types of values inside a single double type. To encode a value as a NaN value, consider the following:
```
                         quit bit and Intel bit
                         v v
|?|1|1|1|1|1|1|1|1|1|1|1|1|1|?|?|?|?|...|?|?|?|
   ^^^^^^^^^^^^^^^^^^^^^     ^^^^^^^^^^^^^^^^^
          NaN bits                 50 bits to
 ^                                  encode
 1 bit                        
 to encode
```
This method allows not only the encoding of every possible 64-bit double precision floating point number, but also the boolean `true`/`false` values, `nil` and generic C pointers that point to values on the heap (because pointers in C are 64 bit values but only contain 48 bits of memory, which fits our total of 51 bits to encode).

## Quick Start
```
$ make
$ ./yabil <filename>
```
Provide filename to execute script or run without filename to run the REPL (Run Execute Print Loop) environment 
