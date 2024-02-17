#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#define DynArr(X) X*

#define EPSILON (Range){0}

#define CONTEXT_ARROW  "\x1b[1;91m^\x1b[m"
#define ERROR_PREFIX   "\x1b[31;1;5mError:\x1b[m "


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

typedef struct {
    char op;
    int source_location;
} OpToken;
