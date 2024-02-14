#include <stdbool.h>
#include <stdint.h>
#define DynArr(X) X*

#define EPSILON 0

typedef struct Node{
    DynArr(uint32_t) dest;
    DynArr(char) sym;
    bool final;
}Node;

typedef struct{
    uint32_t start;
    uint32_t end;
}SubExpr;
