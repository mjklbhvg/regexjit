#include <assert.h>
#include <stdio.h>
#include "regexjit.h"
#include "stb_ds.h"

#define EPSILON (Range){0}

#define CONTEXT_ARROW  "\x1b[1;91m^\x1b[m"
#define ERROR_PREFIX   "\x1b[31;1;5mError:\x1b[m "

typedef struct{
    uint32_t start; // inclusive
    uint32_t end;   // inclusive
}SubExpr;

typedef struct{
    char op;
    int source_location;
}OpToken;

typedef struct{
    char *regex;
    DynArr(OpToken) op_stack;
    DynArr(SubExpr) expr_stack;
    DynArr(Node)    nodes;
}ParserState;

void parseerror(char* msg, char *regex, int location){
    fprintf(stderr, "%s\n%*s" CONTEXT_ARROW "\n" ERROR_PREFIX "%s\n",
        regex ? regex : "", location, "", msg);
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
    // Not overlapping
    arrpush((*nodes)[n1].sym, r);
    arrpush((*nodes)[n1].dest, n2);
}

#define ADD_NODE_PAIR(nodes, n1, n2) \
        uint32_t n1 = arrlen(nodes); \
        arrpush(nodes, (Node){0}); \
        uint32_t n2 = arrlen(nodes); \
        arrpush(nodes, (Node){0});

int apply_op(ParserState *p, OpToken op){
    if(arrlen(p->expr_stack) < ((op.op == '^' || op.op == '|') ? 2 : 1)) {
        parseerror("Operator expected an argument", p->regex, op.source_location);
        return -1;
    }

    SubExpr e = p->expr_stack[arrlen(p->expr_stack)-1];

    switch(op.op){
        case '*':{
            add_trans(&p->nodes, e.start, e.end, EPSILON);
            add_trans(&p->nodes, e.end, e.start, EPSILON);
        }break;
        case '+':{
            add_trans(&p->nodes, e.end, e.start, EPSILON);
        }break;
        case '?':{
            add_trans(&p->nodes, e.start, e.end, EPSILON);
        }break;
        case '^':{
            SubExpr b = arrpop(p->expr_stack);
            SubExpr a = arrpop(p->expr_stack);
            add_trans(&p->nodes, a.end, b.start, EPSILON);
            arrpush(p->expr_stack, ((SubExpr){a.start, b.end}));
        }break;
        case '|':{
            SubExpr b = arrpop(p->expr_stack);
            SubExpr a = arrpop(p->expr_stack);

            ADD_NODE_PAIR(p->nodes, start, end);

            add_trans(&p->nodes, start, a.start, EPSILON);
            add_trans(&p->nodes, start, b.start, EPSILON);
            add_trans(&p->nodes, a.end, end, EPSILON);
            add_trans(&p->nodes, b.end, end, EPSILON);
            arrpush(p->expr_stack, ((SubExpr){start, end}));
        }break;
        default: assert(false);
    }
    return 0;
}

int push_op(ParserState *p, OpToken op){
    while(
        arrlen(p->op_stack) &&
        prec_of(op.op) <= prec_of(p->op_stack[arrlen(p->op_stack)-1].op)
    ){
        if (apply_op(p, arrpop(p->op_stack)) < 0)
            return -1;
    }
    arrpush(p->op_stack, op);
    return 0;
}


DynArr(Node) parse(char *str){
    ParserState p = {0};
    p.regex = str;
    arrpush(p.nodes, (Node){0});

    #define push_op(c) do {\
            if (push_op(&p, (OpToken){c, i}) < 0) return 0; \
        } while (0)

    bool should_concat = false;
    for(uint32_t i = 0; str[i]; i++){
        char c = str[i];
        switch(c){
            case '?':
            case '+':
            case '*':
            case '|':{
                should_concat = c != '|';
                push_op(c);
            }break;
            case '(':{
                if(should_concat) push_op('^');
                should_concat = false;
                arrpush(p.op_stack, ((OpToken){'(', i}));
            }break;
            case ')':{
                should_concat = true;
                while (
                    arrlen(p.op_stack)
                    && '(' != p.op_stack[arrlen(p.op_stack)-1].op
                ) {
                    if (apply_op(&p, arrpop(p.op_stack)) < 0)
                        goto fail;
                }
                if(!(arrlen(p.op_stack))){
                    parseerror("missmatched parens, missing '('", str, i);
                    goto fail;
                }
                assert(arrpop(p.op_stack).op == '(');
            }break;
            case '[':{
                if(should_concat) push_op('^');
                should_concat = true;

                ADD_NODE_PAIR(p.nodes, start, end);
                arrpush(p.expr_stack, ((SubExpr){start, end}));

                while (str[++i] != ']') {
                    Range r;
                    if(!str[i]) {
                        parseerror("invalid char range, expected ']'", str, i);
                        goto fail;
                    }
                    r.start = str[i++];
                    if(str[i++] != '-' || !str[i]) {
                        parseerror("invalid char range, expected '-'", str, i - 1);
                        goto fail;
                    }
                    r.end = str[i];
                    if (r.end < r.start) {
                        parseerror("invalid char range, expected (start <= end)", str, i - 1);
                        goto fail;
                    }
                    add_trans(&p.nodes, start, end, r);
                }
            }break;
            case '.':{
                if(should_concat) push_op('^');
                should_concat = true;

                ADD_NODE_PAIR(p.nodes, start, end);
                add_trans(&p.nodes, start, end, (Range){1, 0xff});
                arrpush(p.expr_stack, ((SubExpr){start, end}));

            }break;
            case '\\':
                c = str[++i];
                if(!c) {
                    parseerror("imagine escaping the null terminator", str, i - 1);
                    goto fail;
                }
            default:{
                if(should_concat) push_op('^');
                should_concat = true;

                ADD_NODE_PAIR(p.nodes, start, end)
                add_trans(&p.nodes, start, end, (Range){c, c});
                arrpush(p.expr_stack, ((SubExpr){start, end}));

                if (((c >> 6) & 3) == 2) {
                    apply_op(&p, (OpToken){'^', i});
                    assert(arrpop(p.op_stack).op == '^');
                }
            }break;
        }
    }
    while(arrlen(p.op_stack)) {
        OpToken to_apply = arrpop(p.op_stack);
        if (to_apply.op == '(') {
            parseerror("mismatched parens, missing ')'", str, to_apply.source_location);
            goto fail;
        }
        if (apply_op(&p, to_apply) < 0)
            goto fail;
    }

    if (!arrlen(p.expr_stack)) {
        parseerror("expression must be nonempty", str, 0);
        goto fail;
    }

    SubExpr e = arrpop(p.expr_stack);
    assert(!arrlen(p.expr_stack)); // Not sure if this can actually happen
    add_trans(&p.nodes, 0, e.start, EPSILON);
    p.nodes[e.end].final = 1;
    arrfree(p.expr_stack);
    arrfree(p.op_stack);
    return p.nodes;

    fail:
        arrfree(p.expr_stack);
        arrfree(p.op_stack);
        free_nodes(p.nodes);
        return 0;

    #undef push_op
    #undef ADD_NODE_PAIR
}

void escape_char(char *buf, size_t s, unsigned char c) {
    snprintf(buf, s,
             c == 0 ? "ε" :
             c == '"' || c == '\\' ? "\\%c" :
             c <= ' ' || c > '~' ? "0x%02x" :
             "%c",
             c
    );
}

// This assumes that state 0 is the only start state.
void dump_graphviz(FILE *f, DynArr(Node) nodes) {
    char lbl_from[8], lbl_to[8];

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
            i ? "#00a3cc" : "#e33400"  ,
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
}

