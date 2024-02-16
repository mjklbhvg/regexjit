#pragma once

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct { const char *errmsg; } OptGetResult;
typedef OptGetResult (OptGetParser)(char *arg, void *userdata);

typedef struct {
    // set either to 0 to only specify a short or long opt
    short shortopt;
    char *longopt;

    char *helptext;

    // NULL means userdata is a bool ptr flag
    OptGetParser *parser;
    void *userdata;
} OptGetSpec;

#define OptGetErr(msg) ((OptGetResult){.errmsg=msg,})
#define OptGetOk ((OptGetResult){.errmsg=NULL,})
#define optget(specs) \
    __optget_internal(argc, argv, sizeof(specs) / sizeof(OptGetSpec), specs)

#define __OPTGET_PROGNAME (argc ? argv[0] : "")
#define __OPTGET_OPTNAME (spec.longopt ? spec.longopt : (char *)&spec.shortopt)

#define __OPTGET_TRY(arg, expr) { \
    OptGetResult r = expr; \
    if (r.errmsg) { \
        fprintf(stderr, "\x1b[1;34m%s\x1b[m: Unexpected '\x1b[31m%s\x1b[m': \x1b[33m%s\x1b[m\n", \
            __OPTGET_PROGNAME, arg, r.errmsg); \
        fail = true; \
    } \
}

#define __OPTGET_TRY_ARG(expr) { \
    OptGetResult r = expr; \
    if (r.errmsg) { \
        fprintf(stderr, "\x1b[1;34m%s\x1b[m: Unexpected '\x1b[31m%s\x1b[m' for option `\x1b[32m%s\x1b[m`: \x1b[33m%s\x1b[m\n", \
            __OPTGET_PROGNAME, argv[i], __OPTGET_OPTNAME, r.errmsg); \
        fail = true; \
    } \
}

#define __OPTGET_HANDLE_FLAG \
    if (!spec.parser) { \
        *((bool *)spec.userdata) = true; \
        continue; \
    }

#define __OPTGET_CHECK_ARG \
    if (i + 1 == argc) { \
        fprintf(stderr, "\x1b[1;34m%s\x1b[m: option `\x1b[32m%s\x1b[m` requires an argument.\n", \
        __OPTGET_PROGNAME, __OPTGET_OPTNAME); \
        fail = true; \
        continue; \
    } \


void __optget_print_help(int argc, char **argv, size_t num_specs, OptGetSpec specs[]) {
    bool redefined_h = false, redefined_help = false;
    printf("Usage: \x1b[1;34m%s\x1b[m: %s\n", __OPTGET_PROGNAME, specs[0].helptext);
    if (num_specs <= 1)
        exit(0);
    size_t s;
    OptGetSpec spec;
    for (spec = specs[s = 1]; s < num_specs; spec = specs[++s]) {
        printf("  ");
        if (spec.shortopt) {
            printf("\x1b[32m-%c\x1b[m", spec.shortopt);
            if ('h' == spec.shortopt)
                redefined_h = true;
        }
        if (spec.longopt) {
            printf("%s\x1b[32m--%s\x1b[m", spec.shortopt ? ", " : "", spec.longopt);
            if (!strcmp("help", spec.longopt))
                redefined_help = true;
        }
        printf("\n\t%s\n", spec.helptext);
    }
    if (redefined_h && redefined_help)
        exit(0);
    printf("  ");
    if (!redefined_h)
        printf("\x1b[32m-h\x1b[m");
    if (!redefined_help)
        printf("%s\x1b[32m--help\x1b[m", !redefined_h ? ", " : "");
    printf("\n\tShow this help message.\n");
    exit(0);
}

bool __optget_internal(int argc, char **argv, size_t num_specs, OptGetSpec specs[]) {
    assert(num_specs && specs[0].parser);

    bool fail = false;

    for (int i = 1; i < argc; i++) {
        bool is_help = false;
        char *arg = argv[i];

        // Handle non-option arguments
        if (*arg != '-') {
            __OPTGET_TRY(arg, specs[0].parser(arg, specs[0].userdata));
            continue;
        }

        size_t s;
        OptGetSpec spec;
        if (arg[1] == '-') {
            // long option
            for (spec = specs[s = 1]; s < num_specs; spec = specs[++s]) {
                if (!spec.longopt || strcmp(spec.longopt, &arg[2])) {
                    if (!strcmp("help", &arg[2])) is_help = true;
                    continue;
                }
                break;
            }
            if (s >= num_specs) {
                if (is_help)
                    __optget_print_help(argc, argv, num_specs, specs);
                else
                    __OPTGET_TRY(arg, specs[0].parser(arg, specs[0].userdata))
            } else {
                __OPTGET_HANDLE_FLAG
                __OPTGET_CHECK_ARG
                __OPTGET_TRY_ARG(spec.parser(argv[++i], spec.userdata))
            }
        } else {
            while (*(++arg)) {
                for (spec = specs[s = 1]; s < num_specs; spec = specs[++s]) {
                    if (!spec.shortopt || spec.shortopt != *arg) {
                        if ('h' == *arg) is_help = true;
                        continue;
                    }
                    break;
                }
                if (s >= num_specs) {
                    if (is_help)
                        __optget_print_help(argc, argv, num_specs, specs);
                    else {
                        char buf[2];
                        buf[0] = *arg;
                        __OPTGET_TRY(buf, specs[0].parser(buf, specs[0].userdata));
                    }
                } else {
                    __OPTGET_HANDLE_FLAG
                    __OPTGET_CHECK_ARG
                    __OPTGET_TRY_ARG(spec.parser(argv[++i], spec.userdata))
                }
            }
        }
    }
    return !fail;
}

// Parses an integer option using strtol
OptGetResult ogp_int(char *arg, void *dest) {
    char *end;
    int i = (int)strtol(arg, &end, 10);
    if (*end)
        return OptGetErr("Failed to parse integer.");
    *(int *)dest = i;
    return OptGetOk;
}

// Parse an integer and assert its > 0
OptGetResult ogp_positive_int(char *arg, void *dest) {
    OptGetResult r = ogp_int(arg, dest);
    if (r.errmsg) return r;
    if (*(int *)dest <= 0) {
        return OptGetErr("Expected positive integer.");
    }
    return OptGetOk;
}

// Parse an integer and assert its >= 0
OptGetResult ogp_nonneg_int(char *arg, void *dest) {
    OptGetResult r = ogp_int(arg, dest);
    if (r.errmsg) return r;
    if (*(int *)dest < 0) {
        return OptGetErr("Expected non negative integer.");
    }
    return OptGetOk;
}

// Store a pointer to the argument in dest
OptGetResult ogp_id(char *arg, void *dest) {
    *((char **) dest) = arg;
    return OptGetOk;
}

// Always fails with the message 'Unknown Argument'
OptGetResult ogp_fail(char *arg, void *dest) {
    (void)arg; (void)dest;
    return OptGetErr("Unknown Argument.");
}
