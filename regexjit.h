#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#define DynArr(X) X*

#define EPSILON (Range){0}

typedef struct {
    size_t state_count;
    uint16_t mat[];
} DfaMat;

typedef struct {
    unsigned char start, end;
} Range;

typedef struct Node{
    DynArr(uint32_t) dest;
    DynArr(Range) sym;
    bool final;
}Node;

typedef struct{
    uint32_t start;
    uint32_t end;
}SubExpr;
