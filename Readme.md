# regexjit

This is an attempt to compile regular expressions to machine code.

## overview
### parse.c
Parser that outputs nfas for given input regexprs.
Use the '-n' option to output the nfas in dot representation.

Supported syntax:
- +, *, ?, |, .
- [a-zA-Z] or [^0-9] like character classes (only allowed for byte ranges)

### main.c
Takes the nfas and turns them into minimal dfas using
https://en.wikipedia.org/wiki/DFA_minimization#Brzozowski's_algorithm.
Output minimal dfas using the '-d' option.

### compile.c
generate x86_64 code for a given dfa.
For each state in the dfa, a series of nested if expressions will
be generated to determine the next state to transition to.
You can output the pseudocode for each state using the '-v' option
and write the generated code using '-o'.

## build and run
```
make
./main '(a|b)+'
```
Every line of input will be matched against each regular expression argument
by both a very simple transition matrix based dfa execution and by running
the generated code.
