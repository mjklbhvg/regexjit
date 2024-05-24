#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#define DynArr(X) X*

typedef struct {
    size_t state_count;
    uint16_t mat[];
} DfaMat;

typedef struct {
    unsigned char start, end; // both inclusive
} Range;

typedef struct Node{
    DynArr(uint32_t) dest;
    DynArr(Range) sym;
    bool final;
}Node;

typedef bool (*compiled_regex_fn)(char* str);

compiled_regex_fn compile_regex(DynArr(Node) *dfa, size_t *len, bool verbose);

void dump_graphviz(FILE *f, DynArr(Node) nodes);

DynArr(Node) parse(const char *str);

void add_trans(DynArr(Node) *nodes, uint32_t n1, uint32_t n2, Range r);

void free_nodes(DynArr(Node) nodes);
