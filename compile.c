#include "regexjit.h"
#include "stb_ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define FAILSTATE (uint32_t)-1

void sort_range(Range* sym, uint32_t* dest, int l, int r){
    #define swap(a,b) {\
        uint32_t tmp_dest = dest[a];\
        Range tmp_sym = sym[a];\
        dest[a] = dest[b];\
        sym[a] = sym[b];\
        dest[b] = tmp_dest;\
        sym[b] = tmp_sym;\
    }
    if(l < r){
        unsigned char p = sym[r].start;
        int i = l-1;
        int j = r;

        do {
            do {
                i++;
            } while (sym[i].start < p);
            do {
                j--;
            } while (j >= l && sym[j].start > p);
            if (i < j) {
                swap(i, j);
            }
        } while (i < j);
        swap(i, r);
        sort_range(sym, dest, l, i - 1);
        sort_range(sym, dest, i + 1, r);
    }
    #undef swap
}

//unsigned char next_bytes(DynArr(unsigned char) *code, )

typedef struct{
    DynArr(uint32_t) jmp_relocs;
    DynArr(uint32_t) labels;
    DynArr(unsigned char) code;
}CodegenState;


void asm_load_next_char(CodegenState *state, bool is_final){
    fprintf(stderr, "load next char: final: %d\n", is_final);
}
void asm_cmp(CodegenState *state, unsigned char imm){
    fprintf(stderr, "cmp REG, %hhu (%c)\n", imm, imm);
}
uint32_t asm_jmpg(CodegenState *state, uint32_t label){
    fprintf(stderr, "jmpg to %u\n", label);
    return 0;
}
void fix_jmp(CodegenState *state, uint32_t reloc, uint32_t code_loc){
    fprintf(stderr, "fixing jmp at %u. now jumps to %u\n", reloc, code_loc);
}

void gen_transitions(CodegenState *state, unsigned char *mins, uint32_t *dest, int l, int r){
    if(l>=r){
        printf("i: %d %d\n", l, r);
        printf("end dings\n");
        return;
    }
    int i = (l+r)/2;
    printf("i: %d\n", i);
    asm_cmp(state, mins[i]);
    uint32_t reloc = asm_jmpg(state, 0);
    gen_transitions(state, mins, dest, l, i-1);
    fix_jmp(state, reloc, arrlen(state->code));
    gen_transitions(state, mins, dest, i+1, r);
}


compiled_regex_fn compile_regex(DynArr(Node) *dfa){


    CodegenState state = {0};

    for (int i = 0; i < arrlen(*dfa); i++) {
        printf("---------- State %d ---------------\n", i);
        // DynArrs are never resized so this is fine
        DynArr(Range) sym = (*dfa)[i].sym;
        DynArr(uint32_t) dest = (*dfa)[i].dest;
        sort_range(sym, dest, 0, arrlen(sym)-1);

        arrpush(state.labels, arrlen(state.code));
        bool is_final = (*dfa)[i].final;
        asm_load_next_char(&state, is_final);

        // numbers for which a <= check needs to be generated
        DynArr(unsigned char) ends = 0;
        DynArr(uint32_t) states = 0;
        unsigned char prev = 0;
        for (int i = 0; i < arrlen(dest); i++) {
            if(sym[i].start != prev+1){
                arrpush(ends, sym[i].start-1);
                arrpush(states, FAILSTATE);
            }
            arrpush(ends, sym[i].end);
            arrpush(states, dest[i]);
            prev = sym[i].end;
        }
        printf("arr: ");
        for (int i = 0; i < arrlen(states); i++) {
            printf("(%c, %d) ", ends[i], states[i]);
        }
        printf("\n");
        gen_transitions(&state, ends, states, 0, arrlen(ends)-1);
    }


    //assert(!"not implemented");
    return 0;
}
