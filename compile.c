#include "regexjit.h"
#include "stb_ds.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define FAILSTATE (uint32_t)-1

#define LOG(...) do { if (VERBOSE) fprintf(stderr, __VA_ARGS__); } while (0)
#define LOG_INDENTWIDTH 3

bool VERBOSE = false;
int DEPTH = 0;

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

typedef struct{
    // offsets into code, where we jump to states
    DynArr(uint32_t) jmp_relocs;
    // offsets into code, where dfa states start
    DynArr(uint32_t) labels;
    // generated machine code
    DynArr(unsigned char) code;
}CodegenState;

extern unsigned char load_next_char_nonfinalstate_template;
extern uint32_t load_next_char_nonfinalstate_template_len;
extern unsigned char load_next_char_finalstate_template;
extern uint32_t load_next_char_finalstate_template_len;

void asm_zero_rax(CodegenState *state){
    // no rex prefix is needed
    // assigning to eax zeros the upper bytes of rax

    // xor eax, eax
    arrpush(state->code, 0x33);
    arrpush(state->code, 0xC0); //0b11000000
}

void asm_load_next_char(CodegenState *state, bool is_final){
    LOG("%*sLOAD NEXT (final: %s)\n", DEPTH, "", is_final ? "true" : "false");

    uint32_t codelen;
    unsigned char* code;
    if(is_final){
        codelen = load_next_char_finalstate_template_len;
        code = &load_next_char_finalstate_template;
    }else{
        codelen = load_next_char_nonfinalstate_template_len;
        code = &load_next_char_nonfinalstate_template;
    }

    uint32_t len = arrlen(state->code);
    arrsetlen(state->code, len + codelen);
    memcpy(state->code + len, code, codelen);
}
void asm_cmp(CodegenState *state, unsigned char imm){
    // cmp al, imm8
    arrpush(state->code, 0x3c);
    arrpush(state->code, imm);
}
uint32_t asm_jmpg(CodegenState *state, uint32_t label){
    // jg rel32
    arrpush(state->code, 0x0f);
    arrpush(state->code, 0x8f);
    for (int i = 0; i < 4; i++) {
        arrpush(state->code, ((unsigned char*)&label)[i]);
    }
    return arrlen(state->code) - 4;
}
void fix_jmp(CodegenState *state, uint32_t reloc, uint32_t code_loc){
    //fprintf(stderr, "fixing jmp at %u. now jumps to %u\n", reloc, code_loc);
    uint32_t rel = code_loc - reloc - 4;
    memcpy(state->code + reloc, &rel, sizeof rel);
}

void jmp_state(CodegenState *state, uint32_t dest){
    if(dest == FAILSTATE){
        LOG("%*sJMP FAILSTATE\n", DEPTH, "");
        // here we need to zero al because it holds the input char
        asm_zero_rax(state);

        // ret
        arrpush(state->code, 0xc3);
    }else{
        LOG("%*sJMP STATE_%d\n", DEPTH, "", dest);
        // jmp rel32
        arrpush(state->code, 0xe9);
        arrpush(state->jmp_relocs, arrlen(state->code));
        for (int i = 0; i < 4; i++) {
            arrpush(state->code, ((unsigned char*)&dest)[i]);
        }
    }
}

void gen_transitions(CodegenState *state, unsigned char *mins, uint32_t *dest, int l, int r){
    if(l>r){
        jmp_state(state, dest[l]);
        return;
    }
    int i = (l+r)/2;
    LOG("%*sIF input <= %d {\n", DEPTH, "", mins[i]);
    asm_cmp(state, mins[i]);
    uint32_t reloc = asm_jmpg(state, 0);
    if (VERBOSE) DEPTH += LOG_INDENTWIDTH;
    gen_transitions(state, mins, dest, l, i-1);

    LOG("%*s} ELSE {\n", DEPTH - LOG_INDENTWIDTH, "");
    fix_jmp(state, reloc, arrlen(state->code));
    gen_transitions(state, mins, dest, i+1, r);

    if (VERBOSE) DEPTH -= LOG_INDENTWIDTH;
    LOG("%*s}\n", DEPTH, "");
}


compiled_regex_fn compile_regex(DynArr(Node) *dfa, size_t *len, bool verbose){
    VERBOSE = verbose;

    CodegenState state = {0};

    // zero rax so that it doesn't have to be done to return 0 on mach fail
    asm_zero_rax(&state);

    for (int i = 0; i < arrlen(*dfa); i++) {
        LOG("--------------------- State %d ---------------------\n", i);
        // DynArrs are never resized so this is fine
        DynArr(Range) sym = (*dfa)[i].sym;
        DynArr(uint32_t) dest = (*dfa)[i].dest;
        sort_range(sym, dest, 0, arrlen(sym)-1);

        arrpush(state.labels, arrlen(state.code));
        bool is_final = (*dfa)[i].final;
        asm_load_next_char(&state, is_final);

        // numbers for which a <= check needs to be generated
        DynArr(unsigned char) ends = 0;
        // states all the ranges go to
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
        arrpush(states, FAILSTATE); // there is 1 more range than there are checks
        LOG("// segmented state transitions: (segment end, destination state)\n//");
        for (int i = 0; i < arrlen(ends); i++) {
            LOG("(%c, %d) ", ends[i], states[i]);
        }
        LOG("\n");
        gen_transitions(&state, ends, states, 0, arrlen(ends)-1);
    }

    // resolve labels
    for (int i = 0; i < arrlen(state.jmp_relocs); i++) {
        uint32_t label;
        memcpy(&label, state.code + state.jmp_relocs[i], sizeof label);
        fix_jmp(&state, state.jmp_relocs[i], state.labels[label]);
    }

    void* memory =
        mmap(0, arrlen(state.code), PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(memory==MAP_FAILED){
        perror("mmap");
        exit(1);
    }
    memcpy(memory, state.code, arrlen(state.code));
    mprotect(memory, arrlen(state.code), PROT_EXEC);

    *len = arrlen(state.code);

    LOG("Generated %lu bytes of code\n", arrlen(state.code));

    return *(compiled_regex_fn *)&memory;
}
