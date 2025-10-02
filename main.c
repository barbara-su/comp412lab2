#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "ir.h"
#include "parser.h"
#include "scanner.h"

int error_flag = 0;

typedef enum {
    MODE_SCAN,
    MODE_PARSE,
    MODE_PRINT,
    MODE_RENAME,
    MODE_HELP
} Mode;

static void print_usage() {
    printf("COMP 412, Fall 2025, Front End  (412fe)\n");
    printf("Command Syntax:\n");
    printf("    412fe [flags] filename\n\n");

    printf("Required arguments:\n");
    printf("    filename  is the pathname (absolute or relative) to the input file\n\n");

    printf("Optional flags:\n");
    printf("\t-h\t prints this message\n");

    printf("At most one of the following flags:\n");
    printf("\t-s\t prints tokens in token stream\n");
    printf("\t-p\t invokes parser and reports on success or failure (default)\n");
    printf("\t-r\t prints human readable version of parser's IR\n");
    printf("\t-x\t runs renaming pass and prints renamed IR (SR/VR/NU)\n");
}

static const char* token_name(TokenType t) {
    switch (t) {
        case TOK_MEMOP:
            return "MEMOP";
        case TOK_LOADI:
            return "LOADI";
        case TOK_ARITHOP:
            return "ARITHOP";
        case TOK_OUTPUT:
            return "OUTPUT";
        case TOK_NOP:
            return "NOP";
        case TOK_CONST:
            return "CONST";
        case TOK_REG:
            return "REG";
        case TOK_COMMA:
            return "COMMA";
        case TOK_INTO:
            return "INTO";
        case TOK_EOF:
            return "ENDFILE";
        case TOK_EOL:
            return "NEWLINE";
        case TOK_ERR:
            return "ERR";
        default:
            return "UNKNOWN";
    }
}

static const char* arithop_lexeme(int val) {
    switch (val) {
        case ARITH_ADD:
            return "add";
        case ARITH_SUB:
            return "sub";
        case ARITH_MULT:
            return "mult";
        case ARITH_LSHIFT:
            return "lshift";
        case ARITH_RSHIFT:
            return "rshift";
        default:
            return "?";
    }
}

static const char* memop_lexeme(int val) {
    switch (val) {
        case LOAD:
            return "load";
        case STORE:
            return "store";
        default:
            return "?";
    }
}

void run_scanner(void) {
    Token tok;
    do {
        tok = get_next_token();

        char buf[64];
        printf("%d: < %s, \"", tok.line, token_name(tok.type));

        switch (tok.type) {
            case TOK_CONST:
                snprintf(buf, sizeof(buf), "%d", tok.value);
                printf("%s", buf);
                break;
            case TOK_REG:
                snprintf(buf, sizeof(buf), "r%d", tok.value);
                printf("%s", buf);
                break;
            case TOK_ARITHOP:
                printf("%s", arithop_lexeme(tok.value));
                break;
            case TOK_MEMOP:
                printf("%s", memop_lexeme(tok.value));
                break;
            case TOK_LOADI:
                printf("loadI");
                break;
            case TOK_OUTPUT:
                printf("output");
                break;
            case TOK_NOP:
                printf("nop");
                break;
            case TOK_COMMA:
                printf(",");
                break;
            case TOK_INTO:
                printf("=>");
                break;
            case TOK_EOL:
                printf("\\n");
                break;
            case TOK_EOF:
                break;
            default:
                printf("???");
                break;
        }
        printf("\" >\n");
    } while (tok.type != TOK_EOF);
}

int main(int argc, char* argv[]) {
    Mode mode = MODE_PARSE;  // default to parse mode
    const char* filename = NULL;

    int opt;
    int hflag = 0, sflag = 0, pflag = 0, rflag = 0, xflag = 0;

    opterr = 0;

    // add 'x' to getopt string
    while ((opt = getopt(argc, argv, "hsprx")) != -1) {
        switch (opt) {
            case 'h':
                hflag = 1;
                break;
            case 's':
                sflag = 1;
                break;
            case 'p':
                pflag = 1;
                break;
            case 'r':
                rflag = 1;
                break;
            case 'x':
                xflag = 1;
                break;
            default:
                fprintf(stderr, "ERROR: Unknown option\n");
                print_usage();
                return EXIT_FAILURE;
        }
    }

    // ensure at most one of -s -p -r -x is used
    int flag_count = sflag + pflag + rflag + xflag + hflag;
    if (flag_count > 1) {
        fprintf(stderr,
                "ERROR:  Multiple command-line flags found.\n"
                "        Try '-h' for information on command-line syntax.\n\n");
        return EXIT_FAILURE;
    }

    // decide mode
    if (hflag) {
        mode = MODE_HELP;
    } else if (xflag) {
        mode = MODE_RENAME;
    } else if (rflag) {
        mode = MODE_PRINT;
    } else if (pflag) {
        mode = MODE_PARSE;
    } else if (sflag) {
        mode = MODE_SCAN;
    } else {
        mode = MODE_PARSE;
    }

    if (mode == MODE_HELP) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (optind < argc) {
        filename = argv[optind];
    }

    if (!filename) {
        fprintf(stderr, "ERROR: Missing filename\n");
        print_usage();
        return EXIT_FAILURE;
    }

    FILE* f_in = fopen(filename, "r");
    if (!f_in) {
        fprintf(stderr, "ERROR: Could not open file '%s'\n", filename);
        print_usage();
        return EXIT_FAILURE;
    }

    // initialize scanner & IR state
    sb_init(f_in);
    init_pool_list();
    init_node_list();

    if (mode == MODE_SCAN) {
        run_scanner();
    } else if (mode == MODE_PARSE) {
        int count;
        parse_program(&count);
        if (!error_flag) {
            printf("Parse succeeded. Processed %d operations.\n", count);
        } else {
            printf("Parse found errors.\n");
        }
    } else if (mode == MODE_PRINT) {
        int count;
        parse_program(&count);
        if (!error_flag) {
            printf("Parse succeeded. Processed %d operations.\n", count);
            ir_print();
        } else {
            printf("\nDue to syntax error(s), run terminates.\n");
        }
    } else if (mode == MODE_RENAME) {  // -x flag supported
        int count;
        parse_program(&count);
        if (!error_flag) {
            int maxlive = 0;
            ir_rename(&maxlive);
            ir_print();
        } else {
            printf("\nDue to syntax error(s), run terminates.\n");
        }
    }

    sb_free();
    fclose(f_in);
    return EXIT_SUCCESS;
}
