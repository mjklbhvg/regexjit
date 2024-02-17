#include "regexjit.h"
#include "stb_ds.h"
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>


void sort_range(Range* sym, uint32_t* dest, int len){
    if(len <= 1) return;
    int a = 0;
    int p = len-1;
    int b = len-2;
    while(b > a){
        while(sym[a].start < sym[p].start){
            a++;
            if(b == a) goto end;
        }
        while(sym[b].start > sym[p].start){
            b--;
            if(b == a) goto end;
        }
        Range tmp_sym = sym[a];
        uint32_t tmp_dest = dest[a];
        sym[a] = sym[b];
        dest[a] = dest[b];
        sym[b] = tmp_sym;
        dest[b] = tmp_dest;
    }
    assert(a==b);
    end:{
        Range tmp_sym = sym[p];
        uint32_t tmp_dest = dest[p];
        sym[p] = sym[b];
        dest[p] = dest[b];
        sym[b] = tmp_sym;
        dest[b] = tmp_dest;
    }
    sort_range(sym, dest, b);
    b++;
    sort_range(&sym[b], &dest[b], len-b);
}

void sort_ranges(DynArr(Node) *dfa){
    for (int i = 0; i < arrlen(*dfa); i++) {

        // DynArrs are never resized so this is fine
        DynArr(Range) sym = (*dfa)[i].sym;
        DynArr(uint32_t) dest = (*dfa)[i].dest;
        sort_range(sym, dest, arrlen(sym));
    }
}

compiled_regex_fn compile_regex(DynArr(Node) *dfa){

    DynArr(uint32_t) jmp_relocs = 0;
    DynArr(uint32_t) labels = 0;
    DynArr(unsigned char) code = 0;

    sort_ranges(dfa);


    assert(!"not implemented");
}
