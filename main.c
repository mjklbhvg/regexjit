#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stdint.h>
#include "stb_ds.h"
#include "regexjit.h"
#include "optget.h"

void dump_array(DynArr(uint32_t) a) {
    putchar('{');
    for (int i = 0; i < arrlen(a); ++i != arrlen(a) ? printf(", ") : 0) {
        printf("%d", a[i]);
    }
    putchar('}');
}

#define TRUE_STR "\x1b[32mtrue\x1b[m"
#define FALSE_STR "\x1b[31mfalse\x1b[m"

int64_t insert_sorted(DynArr(uint32_t) *a, uint32_t t) {
    for (int j = 0; j < arrlen(*a); j++) {
        if ((*a)[j] == t)
            return -1;
        if ((*a)[j] > t) {
            arrins(*a, j, t);
            return j;
        }
    }
    arrpush(*a, t);
    return arrlen(*a) - 1;
}

void free_nodes(DynArr(Node) nodes) {
    for (int i = 0; i < arrlen(nodes); i++) {
        arrfree(nodes[i].dest);
        arrfree(nodes[i].sym);
    }
    arrfree(nodes);
}

// Compute the epsilon closure of the list of states 'states' in the nfa 'nfa'
// and add it to the list of states.
// input list must be sorted, output list will be sorted
void epsilon_closure(DynArr(uint32_t) *states, DynArr(Node) nfa) {
    DynArr(uint32_t) worklist = 0;
    for (int i = 0; i < arrlen(*states); i++)
        arrpush(worklist, i);

    while (arrlen(worklist)) {
        uint32_t inner_node_idx = (*states)[arrpop(worklist)];
        for (int out = 0; out < arrlen(nfa[inner_node_idx].dest); out++) {
            // if not an epsilon transition, we can skip it
            if (nfa[inner_node_idx].sym[out].start)
                continue;
            // the state is reachable using only epsilon transitions
            uint32_t reachable_state = nfa[inner_node_idx].dest[out];
            // add it to the inner nodes only if it is not already in there
            // FIXME: this would be better with binary search for larger states
            // FIXME: probably even more better with a bit set
            int64_t idx;
            if ((idx = insert_sorted(states, reachable_state)) >= 0) {
                for (int i = 0; i < arrlen(worklist); i++) {
                    if (worklist[i] >= idx)
                        worklist[i]++;
                }
                arrpush(worklist, idx);
            }
        }
    }
    arrfree(worklist);
}

DynArr(Node) reverse_automaton(DynArr(Node) nfa, DynArr(uint32_t) *start_states) {
    DynArr(Node) inv = 0;
    for (int i = 0; i < arrlen(nfa); i++) {
        arrpush(inv, ((Node){0, 0, !i}));
        // if the node was a final node in the nfa, remember it
        if (nfa[i].final) {
            arrpush(*start_states, i);
        }
    }

    // Now that we have all nodes we can add all the transitions, but swapped

    for (int i = 0; i < arrlen(nfa); i++) {
        for (int j = 0; j < arrlen(nfa[i].dest); j++)
            add_trans(&inv, nfa[i].dest[j], i, nfa[i].sym[j]);
    }
    return inv;
}

DynArr(Node) construct_dfa(DynArr(Node) nfa, DynArr(uint32_t) start_states) {
    // The dfa nodes as well as the corresponding list of "inner" nfa nodes
    DynArr(Node) dfa = 0;
    DynArr(DynArr(uint32_t)) inner = 0;

    // dfa node indecies that we have not found the transitions for yet
    DynArr(uint32_t) worklist = 0;

    // The dfa start state
    arrpush(dfa, (Node){0});

    // contains only the nfa start state for now
    arrpush(inner, 0);
    for (int i = 0; i < arrlen(start_states); i++) {
        arrpush(inner[0], start_states[i]);
    }
    epsilon_closure(&inner[0], nfa);

    arrpush(worklist, 0);

    while (arrlen(worklist)) {
        uint32_t workidx = arrpop(worklist);
        // while we iterate over all inner states of this dfa state
        // we keep track if any final states are found
        bool should_be_final = false;

        // check destination states for each character in the alphabet
        // it would be nicer to iterate over ranges here that can maybe be found
        // by intersecting all the out ranges and removing the resulting range
        // from each out range, repeat until intersection is {}
        // then iterate over all these found intersections and remaining nonempty ranges
        for (int c = 1; c < 0x100; c++) {
            DynArr(uint32_t) dest = 0;
            for (int i = 0; i < arrlen(inner[workidx]); i++) {
                should_be_final |= nfa[inner[workidx][i]].final;

                // if this inner state has some transition on 'c'
                // the destination should be added to dest
                for (int out = 0; out < arrlen(nfa[inner[workidx][i]].sym); out++) {
                    Range r = nfa[inner[workidx][i]].sym[out];
                    if (r.start <= c && r.end >= c) {
                        insert_sorted(&dest, nfa[inner[workidx][i]].dest[out]);
                    }
                }
            }

            // don't need to free dest here :)
            if (!arrlen(dest))
                continue;

            epsilon_closure(&dest, nfa);

            int64_t dfa_idx = -1;

            // check if an equivalent state is already in the dfa
            for (int j = 0; j < arrlen(inner); j++) {
                if (arrlen(dest) == arrlen(inner[j])
                        && !memcmp(dest, inner[j], arrlen(dest) * sizeof(uint32_t))) {
                    dfa_idx = j;
                    arrfree(dest);
                    break;
                }
            }

            // if not, we need to add it it to the dfa and the worklist
            if (dfa_idx == -1) {
                dfa_idx = arrlen(dfa);
                arrpush(dfa, (Node){0});
                arrpush(inner, dest);
                arrpush(worklist, dfa_idx);
            }

            // finally we add the transition between the current dfa state
            // and the destination state with the current character
            add_trans(&dfa, workidx, dfa_idx, (Range){c, c});
        }

        dfa[workidx].final = should_be_final;
    }

    arrfree(worklist);
    for (int i = 0; i < arrlen(inner); i++)
        arrfree(inner[i]);
    arrfree(inner);

    return dfa;
}

OptGetResult push_regex(char *arg, void *dest) {
    DynArr(char *) *lst = dest;
    arrpush(*lst, arg);
    return OptGetOk;
}

OptGetResult open_file(char *arg, void *dest) {
    FILE **f = (FILE **)dest;
    if (!(*f = fopen(arg, "w")))
        return OptGetErr("fopen failed");
    return OptGetOk;
}

// transition matrix has 256 shorts for each state
// idx zero is is_final flag, rest is next state for input bytes 0x1-0xff
DfaMat *construct_transition_matrix(DynArr(Node) dfa) {
    // TODO: check for overflow
    size_t len = sizeof(DfaMat) + arrlen(dfa) * sizeof(short) * 256;
    DfaMat *transitions = malloc(len);
    if (!transitions) return NULL;

    memset(transitions, 0xff, len);

    transitions->state_count = arrlen(dfa);

    for (int i = 0; i < arrlen(dfa); i++) {
        transitions->mat[i*256] = dfa[i].final;
        for (int j = 0; j < arrlen(dfa[i].sym); j++) {
            Range r = dfa[i].sym[j];
            for (int k = r.start; k <= r.end; k++) {
                transitions->mat[i*256 + k] = dfa[i].dest[j];
            }
        }
    }

    return transitions;
}

bool lame_dfa_match(DfaMat *trans, char *str) {
    uint16_t state = 0;
    uint8_t c;
    while ((c = *str++)) {
        state = trans->mat[state * 256 + c];
        if (state == 0xffff)
            return false;
    }
    return trans->mat[state * 256];
}

int main(int argc, char *argv[]) {

    FILE *nfafile = 0, *dfafile = 0, *codefile = 0;
    bool verbose = false;

    DynArr(char *) regexe = 0;
    DynArr(DfaMat *) dfas_mat = 0;
    DynArr(compiled_regex_fn) regex_fns = 0;

    bool r = optget(((OptGetSpec[]) {
        {0, 0, "[-n <file>] [-d <file>] [-o <file> ] [-v] <regex>...", ogp_fail, 0},
        {0, "<regex>", "a regular expression to match against lines of input", push_regex, &regexe},
        {'n', "dump-nfa", "save dot representation of nfa to file", open_file, &nfafile},
        {'d', "dump-dfa", "save dot representation of dfa to file", open_file, &dfafile},
        {'o', "output", "save generated machine code to file", open_file, &codefile},
        {'v', "verbose", "log machine code generation", 0, &verbose}
    }));

    for (int i = 0; i < arrlen(regexe); i++) {
        DynArr(Node) nfa = parse(regexe[i]);
        if (!nfa) {
            arrdel(regexe, i);
            i--;
            continue;
        }
        if (nfafile) dump_graphviz(nfafile, nfa);
        DynArr(uint32_t) start_states = 0;
        DynArr(Node) reverse = reverse_automaton(nfa, &start_states);
        DynArr(Node) reverse_dfa = construct_dfa(reverse, start_states);
        arrfree(start_states);
        start_states = 0;
        DynArr(Node) smaller_nfa = reverse_automaton(reverse_dfa, &start_states);
        DynArr(Node) dfa = construct_dfa(smaller_nfa, start_states);
        if (dfafile) dump_graphviz(dfafile, dfa);

        DfaMat *t = construct_transition_matrix(dfa);
        if (!t) {
            fprintf(stderr, "oom\n");
            exit(1);
        }
        arrpush(dfas_mat, t);

        size_t len;
        compiled_regex_fn f = compile_regex(&dfa, &len, verbose);
        arrpush(regex_fns, f);

        if (codefile)
            fwrite(*(char **)&f, len, 1, codefile);

        arrfree(nfa);
        arrfree(dfa);
    }

    if (!r || !arrlen(regex_fns)) {
        fputs("Need at least one regular expression.\nTry -h for help.\n", stderr);
        return 0;
    }

    if (nfafile) fclose(nfafile);
    if (dfafile) fclose(dfafile);
    if (codefile) fclose(codefile);

    // lol
    if (nfafile || dfafile)
        system("dot -O -Tsvg *.dot");

    char *input = NULL;
    size_t buf_size;
    ssize_t input_size;

    while ((input_size = getline(&input, &buf_size, stdin)) != -1) {
        if (!input_size) continue;
        if (input[input_size - 1] == '\n')
            input[input_size - 1] = '\0';
        printf("\x1b[4mlame dfa match:\x1b[m\t\t\t\x1b[4mcool jit match:\x1b[m");
        for (int i = 0; i < arrlen(dfas_mat); ++i != arrlen(dfas_mat) ? putchar('\t') : 0) {
            printf("\n\x1b[1m%s\x1b[m:\n", regexe[i]);
            struct timespec start_cpu_time, end_cpu_time;

            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_cpu_time) < 0) {
                perror("clock_gettime");
                exit(1);
            }

            bool matches = lame_dfa_match(dfas_mat[i], input);

            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu_time) < 0) {
                perror("clock_gettime");
                exit(1);
            }
            double cpu_time =
                (end_cpu_time.tv_sec - start_cpu_time.tv_sec) * 1000.0 +
                (end_cpu_time.tv_nsec - start_cpu_time.tv_nsec) / 1000000.0;
            printf("%s, %lfms", matches ? TRUE_STR : FALSE_STR, cpu_time);

            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_cpu_time) < 0) {
                perror("clock_gettime");
                exit(1);
            }
            matches = regex_fns[i](input);
            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_cpu_time) < 0) {
                perror("clock_gettime");
                exit(1);
            }

            cpu_time =
                (end_cpu_time.tv_sec - start_cpu_time.tv_sec) * 1000.0 +
                (end_cpu_time.tv_nsec - start_cpu_time.tv_nsec) / 1000000.0;

            printf("\t\t%s, %lfms", matches ? TRUE_STR : FALSE_STR, cpu_time);
        }
        putchar('\n');
    }

    for (int i = 0; i < arrlen(dfas_mat); i++)
        free(dfas_mat[i]);
    arrfree(dfas_mat);
    return 0;
}
