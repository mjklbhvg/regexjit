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
                (uint64_t)&nodes[i].dest[j],
                c == '"' || c == '\\' ? "\\" : "",
                c
            );
        }
    }

    fputc('}', f);
    fclose(f);

    // lol
    system("dot -Tsvg *.dot -O");
}

int main(int argc, char *argv[])
{

    return 0;
}
