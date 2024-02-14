#include <stdbool.h>
#define DynArr(X) X*

typedef struct Node{
    DynArr(struct Node*) dest;
    DynArr(char) sym;
    bool final;
}Node;
