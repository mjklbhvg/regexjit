#include <stdio.h>
#include <stdlib.h>
#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stdint.h>
#include "stb_ds.h"
#include "regexjit.h"

void escape_char(char *buf, size_t s, unsigned char c) {
    snprintf(buf, s,
             c == 0 ? "ε" :
             c == '"' || c == '\\' ? "\\%c" :
             c <= ' ' || c > '~' ? "0x%02x" :
             "%c",
             c
    );
}

void dump_graphviz(char *filename, uint32_t start_state, DynArr(Node) nodes) {
    char lbl_from[8], lbl_to[8];

    FILE *f = fopen(filename, "w");
    fputs("digraph g {\n"
        "\trankdir=LR;\n"
        "\tbgcolor=\"#0f111b\";\n"
        "\tfontcolor=\"#ecf0c1\";\n"
        "\tconcentrate=\"true\";\n",
        f
    );

    for (int i = 0; i < arrlen(nodes); i++) {
        fprintf(f, "\tn%016lx ["
            "label=\"%d\" "
            "color=\"%s\" "
            "shape=\"%s\" "
            "fontcolor=\"#ecf0c1\"];\n",
            (uint64_t)&nodes[i],
            i,
            i != start_state ? "#00a3cc" : "#e33400"  ,
            nodes[i].final ? "doublecircle" : "circle"
        );

        for (int j = 0; j < arrlen(nodes[i].dest); j++) {
            Range r = nodes[i].sym[j];
            escape_char(lbl_from, sizeof(lbl_from), r.start);
            if (r.end != r.start)
                escape_char(lbl_to, sizeof(lbl_to), r.end);
            fprintf(f, "\tn%016lx -> n%016lx ["
                "label=\"%s%s%s\" "
                "color=\"#ecf0c1\" "
                "fontcolor=\"#f2ce00\"];\n",
                (uint64_t)&nodes[i],
                (uint64_t)&nodes[nodes[i].dest[j]],
                lbl_from,
                r.end != r.start ? "-" : "",
                r.end != r.start ? lbl_to : ""
            );
        }
    }

    fputs("}\n", f);
    fclose(f);

    // lol
    system("dot -Tsvg *.dot -O");

}

void add_trans(DynArr(Node) *nodes, uint32_t n1, uint32_t n2, Range r){
    for (int i = 0; i < arrlen((*nodes)[n1].sym); i++) {
        if ((*nodes)[n1].dest[i] != n2)
            continue;
        Range *r2 = &(*nodes)[n1].sym[i];
        // r is included in r2
        if (r2->start <= r.start && r2->end >= r.end)
            return;
        // r2 is included in r
        if (r.start < r2->start && r.end > r2->end) {
            r2->start = r.start;
            r2->end = r.end;
            return;
        }
        // r is overlapping lower
        if (r.start < r2->start && r.end >= r2->start - 1) {
            r2->start = r.start;
            return;
        }
        // r is overlapping upper
        if (r.end > r2->end && r.start <= r2->end + 1) {
            r2->end = r.end;
            return;
        }
    }
    arrpush((*nodes)[n1].sym, r);
    arrpush((*nodes)[n1].dest, n2);
}

int prec_of(char op){
    switch (op) {
        case '*': return 3;
        case '+': return 3;
        case '?': return 3;
        case '^': return 2;
        case '|': return 1;
        case '(': return 0;
        default: assert(false);
    }
}

void parseerror(char* msg){
    fprintf(stderr, "PARSE ERROR: %s\n", msg);
    exit(1);
}

void apply_op(
    char op,
    DynArr(SubExpr) *out_queue,
    DynArr(Node) *nodes
){

    if(!arrlen(*out_queue)){
        parseerror("expected stuff before operator");
    }
    switch(op){
        case '*':{
            SubExpr e = (*out_queue)[arrlen(*out_queue)-1];
            add_trans(nodes, e.start, e.end, EPSILON);
            add_trans(nodes, e.end, e.start, EPSILON);

        }break;
        case '+':{
            SubExpr e = (*out_queue)[arrlen(*out_queue)-1];
            add_trans(nodes, e.end, e.start, EPSILON);
        }break;
        case '?':{
            SubExpr e = (*out_queue)[arrlen(*out_queue)-1];
            add_trans(nodes, e.start, e.end, EPSILON);
        }break;
        case '^':{
            SubExpr b = arrpop(*out_queue);
            if(!arrlen(*out_queue)){
                parseerror("expected snd stuff before operator");
            }
            SubExpr a = arrpop(*out_queue);
            add_trans(nodes, a.end, b.start, EPSILON);
            arrpush(*out_queue, ((SubExpr){a.start, b.end}));
        }break;
        case '|':{
            SubExpr a = arrpop(*out_queue);
            if(!arrlen(*out_queue)){
                parseerror("expected snd stuff before operator");
            }
            SubExpr b = arrpop(*out_queue);

            uint32_t start = arrlen(*nodes);
            arrpush(*nodes, (Node){0});
            uint32_t end = arrlen(*nodes);
            arrpush(*nodes, (Node){0});

            add_trans(nodes, start, a.start, EPSILON);
            add_trans(nodes, start, b.start, EPSILON);
            add_trans(nodes, a.end, end, EPSILON);
            add_trans(nodes, b.end, end, EPSILON);
            arrpush(*out_queue, ((SubExpr){start, end}));
        }break;
    }
}

void push_op(
    char op,
    DynArr(char) *op_stack,
    DynArr(SubExpr) *out_queue,
    DynArr(Node) *nodes
){
    while(arrlen(*op_stack) &&
        prec_of(op) <= prec_of((*op_stack)[arrlen(*op_stack)-1])){
        apply_op(arrpop(*op_stack), out_queue, nodes);
    }
    arrpush(*op_stack, op);
}


DynArr(Node) parse(char *str){
    DynArr(Node) nodes = 0;

    arrpush(nodes, (Node){0});

    #define push_op(c) push_op(c, &op_stack, &out_stack, &nodes);

    DynArr(char) op_stack = 0;
    DynArr(SubExpr) out_stack = 0;

    bool should_concat = false;
    for(char c = *str; *str != 0; c = *++str){
        switch(c){
            case '?':
            case '+':
            case '*':{
                should_concat = true;
                push_op(c);
            }break;
            case '|':{
                should_concat = false;
                push_op(c);
            }break;
            case '(':{
                if(should_concat){
                    push_op('^');
                }
                should_concat = false;
                arrpush(op_stack, '(');
            }break;
            case '[':{
                if(should_concat){
                    push_op('^');
                }
                uint32_t n = arrlen(nodes);
                arrpush(nodes, (Node){0});
                uint32_t n2 = arrlen(nodes);
                arrpush(nodes, (Node){0});

                arrpush(out_stack, ((SubExpr){n, n2}));
                while (*++str != ']') {
                    Range r;
                    if(!*str) parseerror("expected ']'");
                    r.start = *str++;
                    if(*str++ != '-' || !*str) parseerror("expected '-'");
                    r.end = *str;
                    if (r.end < r.start)
                        parseerror("invalid char range");
                    add_trans(&nodes, n, n2, r);
                }
                should_concat = true;
            }break;
            case ')':{
                should_concat = true;

                while(arrlen(op_stack)
                    && '('!=op_stack[arrlen(op_stack)-1]){
                    apply_op(arrpop(op_stack), &out_stack, &nodes);
                }
                if(!(arrlen(op_stack))){
                    parseerror("missmachted parens");
                }
                assert(arrpop(op_stack)=='(');
            }break;
            case '.':{
                if(should_concat){
                    push_op('^');
                }
                uint32_t n = arrlen(nodes);
                arrpush(nodes, (Node){0});
                uint32_t n2 = arrlen(nodes);
                arrpush(nodes, (Node){0});
                add_trans(&nodes, n, n2, (Range){1, 0xff});

                arrpush(out_stack, ((SubExpr){n, n2}));

                should_concat = true;
            }break;
            case '\\':
                c = *++str;
                if(!c) parseerror("imagine escaping the null terminator");
            default:{
                if(should_concat){
                    push_op('^');
                }
                uint32_t n = arrlen(nodes);
                arrpush(nodes, (Node){0});
                uint32_t n2 = arrlen(nodes);
                arrpush(nodes, (Node){0});
                add_trans(&nodes, n, n2, (Range){c, c});

                arrpush(out_stack, ((SubExpr){n, n2}));

                if (((c >> 6) & 3) == 2) {
                    apply_op('^', &out_stack, &nodes);
                    assert(arrpop(op_stack) == '^');
                }

                should_concat = true;
            }break;
        }
    }
    while(arrlen(op_stack)){
        apply_op(arrpop(op_stack), &out_stack, &nodes);
    }

    SubExpr e = arrpop(out_stack);
    add_trans(&nodes, 0, e.start, EPSILON);
    nodes[e.end].final = 1;
    return nodes;
    #undef push_op
}

void dump_array(DynArr(uint32_t) a) {
    putchar('{');
    for (int i = 0; i < arrlen(a); ++i != arrlen(a) ? printf(", ") : 0) {
        printf("%d", a[i]);
    }
    putchar('}');
}

void insert_sorted(DynArr(uint32_t) *a, uint32_t t) {
    for (int j = 0; j < arrlen(*a); j++) {
        if ((*a)[j] == t)
            return;
        if ((*a)[j] > t) {
            arrins(*a, j, t);
            return;
        }
    }
    arrpush(*a, t);
}

// Compute the epsilon closure of the list of states 'states' in the nfa 'nfa'
// and add it to the list of states.
// input list must be sorted, output list will be sorted
void epsilon_closure(DynArr(uint32_t) *states, DynArr(Node) nfa) {
    for (int i = 0; i < arrlen(*states); i++) {
        uint32_t inner_node_idx = (*states)[i];
        for (int out = 0; out < arrlen(nfa[inner_node_idx].dest); out++) {
            // if not an epsilon transition, we can skip it
            if (nfa[inner_node_idx].sym[out].start)
                continue;
            // the state is reachable using only epsilon transitions
            uint32_t reachable_state = nfa[inner_node_idx].dest[out];
            // add it to the inner nodes only if it is not already in there
            // FIXME: this would be better with binary search for larger states
            // FIXME: probably even more better with a bit set
            insert_sorted(states, reachable_state);
        }
    }
}

DynArr(Node) construct_dfa(DynArr(Node) nfa) {
    // The dfa nodes as well as the corresponding list of "inner" nfa nodes
    DynArr(Node) dfa = 0;
    DynArr(DynArr(uint32_t)) inner = 0;

    // dfa node indecies that we have not found the transitions for yet
    DynArr(uint32_t) worklist = 0;

    // The dfa start state
    arrpush(dfa, (Node){0});

    // contains only the nfa start state for now
    arrpush(inner, 0);
    arrpush(inner[0], 0);
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

int main(int argc, char *argv[])
{
    char* regex = "abc";
    if(argc > 1){
        regex = argv[1];
    }
    DynArr(Node) nfa = parse(regex);
    dump_graphviz("nfa.dot", 0, nfa);

    DynArr(Node) dfa = construct_dfa(nfa);

    dump_graphviz("dfa.dot", 0, dfa);

    return 0;
}
