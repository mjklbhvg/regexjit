#include <stdio.h>
#include <stdlib.h>
#define STB_DS_IMPLEMENTATION
#include <stdio.h>
#include <stdint.h>
#include "stb_ds.h"
#include "regexjit.h"

void dump_graphviz(char *filename, uint32_t start_state, DynArr(Node) nodes) {

    FILE *f = fopen(filename, "w");
    fputs("digraph g{"
        "rankdir=LR;"
        "bgcolor=\"#0f111b\";"
        "fontcolor=\"#ecf0c1\";"
        "concentrate=\"true\";",
        f
    );

    for (int i = 0; i < arrlen(nodes); i++) {
        fprintf(f, "n%016lx ["
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
            char c = nodes[i].sym[j];
            fprintf(f, "n%016lx -> n%016lx ["
                "label=\"%s%c\" "
                "color=\"#ecf0c1\" "
                "fontcolor=\"#f2ce00\"];\n",
                (uint64_t)&nodes[i],
                (uint64_t)&nodes[nodes[i].dest[j]],
                c == '"' || c == '\\' ? "\\" : c ? "" : "ε",
                c ? c : ' '
            );
        }
    }

    fputc('}', f);
    fclose(f);

    // lol
    system("dot -Tsvg *.dot -O");

}
void add_trans(DynArr(Node) *nodes, uint32_t n1, uint32_t n2, char c){
    arrpush((*nodes)[n1].sym, c);
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
                printf("c: %c\n", c);
                add_trans(&nodes, n, n2, c);

                arrpush(out_stack, ((SubExpr){n, n2}));

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

int main(int argc, char *argv[])
{
    char* regex = "abc";
    if(argc > 1){
        regex = argv[1];
    }
    DynArr(Node)nodes = parse(regex);
    dump_graphviz("suchfile.dot", 0, nodes);
    return 0;
}
