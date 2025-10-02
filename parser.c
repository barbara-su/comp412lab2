#include "parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "ir.h"

static Token word;
static int opCount;
static Token tb[3];

static void parse_errorf(int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "ERROR %d:\t", line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    error_flag = 1;

    // get next token until this line is flushed
    while (word.type != TOK_EOL && word.type != TOK_EOF) {
        word = get_next_token();
    }
}

const char *token_to_string(Token tok) {
    static char buf[64];

    switch (tok.type) {
        case TOK_CONST:
            snprintf(buf, sizeof(buf), "\"%d\" (CONST)", tok.value);
            break;
        case TOK_REG:
            snprintf(buf, sizeof(buf), "\"r%d\" (REG)", tok.value);
            break;
        case TOK_ARITHOP:
            switch (tok.value) {
                case ARITH_ADD:
                    snprintf(buf, sizeof(buf), "\"add\" (ARITHOP)");
                    break;
                case ARITH_SUB:
                    snprintf(buf, sizeof(buf), "\"sub\" (ARITHOP)");
                    break;
                case ARITH_MULT:
                    snprintf(buf, sizeof(buf), "\"mult\" (ARITHOP)");
                    break;
                case ARITH_LSHIFT:
                    snprintf(buf, sizeof(buf), "\"lshift\" (ARITHOP)");
                    break;
                case ARITH_RSHIFT:
                    snprintf(buf, sizeof(buf), "\"rshift\" (ARITHOP)");
                    break;
                default:
                    snprintf(buf, sizeof(buf), "\"?\" (ARITHOP)");
                    break;
            }
            break;
        case TOK_LOADI:
            snprintf(buf, sizeof(buf), "\"loadI\" (LOADI)");
            break;
        case TOK_MEMOP:
            if (tok.value == LOAD)
                snprintf(buf, sizeof(buf), "\"load\" (MEMOP)");
            else if (tok.value == STORE)
                snprintf(buf, sizeof(buf), "\"store\" (MEMOP)");
            else
                snprintf(buf, sizeof(buf), "\"?\" (MEMOP)");
            break;
        case TOK_OUTPUT:
            snprintf(buf, sizeof(buf), "\"output\" (OUTPUT)");
            break;
        case TOK_NOP:
            snprintf(buf, sizeof(buf), "\"nop\" (NOP)");
            break;
        case TOK_COMMA:
            snprintf(buf, sizeof(buf), "\",\" (COMMA)");
            break;
        case TOK_INTO:
            snprintf(buf, sizeof(buf), "\"=>\" (INTO)");
            break;
        case TOK_EOL:
            snprintf(buf, sizeof(buf), "\"\\n\" (NEWLINE)");
            break;
        case TOK_EOF:
            snprintf(buf, sizeof(buf), "\"EOF\" (ENDFILE)");
            break;
        case TOK_ERR:
            snprintf(buf, sizeof(buf), "\"error\" (ERR)");
            break;
        default:
            snprintf(buf, sizeof(buf), "\"?\" (UNKNOWN)");
            break;
    }

    return buf;
}

void parse_program(int *count) {
    opCount = 0;

    word = get_next_token();
    int line;

    while (word.type != TOK_EOF) {
        line = word.line;
        switch (word.type) {
            case TOK_MEMOP:
                finish_memop(word, line);
                break;
            case TOK_LOADI:
                finish_loadI(line);
                break;
            case TOK_ARITHOP:
                finish_arithop(word, line);
                break;
            case TOK_OUTPUT:
                finish_output(line);
                break;
            case TOK_NOP:
                finish_nop(line);
                break;
            case TOK_EOL:
                break;
            default:
                printf("Unexpected token type %d at line %d\n", word.type, line);
                parse_errorf(line, "unexpected df token at line");
        }
        // parse error may already skip to the next one

        word = get_next_token();
    }
    *count = opCount;
    return;
}

void finish_memop(Token first, int line) {
    word = get_next_token();

    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing source register in load or store.");
        return;
    };
    tb[0] = word;

    word = get_next_token();
    if (word.type != TOK_INTO) {
        parse_errorf(line, "Missing '=>' in load or store.");
        return;
    }

    word = get_next_token();
    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing target register in load or store.");
        return;
    }
    tb[1] = word;

    word = get_next_token();
    if (word.type != TOK_EOL && word.type != TOK_EOF) {
        parse_errorf(line, "Extra token at end of line %s.", token_to_string(word));
        return;
    }

    switch (first.value) {
        case LOAD:
            ir_build(IR_LOAD, line, 2, tb[0].value, tb[1].value);
            break;
        case STORE:
            ir_build(IR_STORE, line, 2, tb[0].value, tb[1].value);
            break;
        default:
            parse_errorf(line, "Unknown memory operation.");
            return;
    }
    opCount++;
}

void finish_loadI(int line) {
    word = get_next_token();

    if (word.type != TOK_CONST) {
        parse_errorf(line, "Missing constant in loadI.");
        return;
    }
    tb[0] = word;

    word = get_next_token();
    if (word.type != TOK_INTO) {
        parse_errorf(line, "Missing '=>' in loadI.");
        return;
    }

    word = get_next_token();
    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing target register in loadI.");
        return;
    }
    tb[1] = word;

    word = get_next_token();
    if (word.type != TOK_EOL && word.type != TOK_EOF) {
        parse_errorf(line, "Extra token at end of line %s.", token_to_string(word));
        return;
    }

    ir_build(IR_LOADI, line, 2, tb[0].value, tb[1].value);

    opCount++;
}

static inline char *arithop_lexeme(int val) {
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

void finish_arithop(Token first, int line) {
    word = get_next_token();
    char *op = arithop_lexeme(first.value);
    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing first source register in %s.", op);
        return;
    }
    tb[0] = word;

    word = get_next_token();
    if (word.type != TOK_COMMA) {
        parse_errorf(line, "Missing comma in %s.", op);
        return;
    }

    word = get_next_token();
    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing second source register in %s.", op);
        return;
    }
    tb[1] = word;

    word = get_next_token();
    if (word.type != TOK_INTO) {
        parse_errorf(line, "Missing '=>' in %s", op);
        return;
    }

    word = get_next_token();
    if (word.type != TOK_REG) {
        parse_errorf(line, "Missing target register in %s.", op);
        return;
    }
    tb[2] = word;

    word = get_next_token();
    if (word.type != TOK_EOL && word.type != TOK_EOF) {
        parse_errorf(line, "Extra token at end of line %s.", token_to_string(word));
        return;
    }

    switch (first.value) {
        case ARITH_ADD:
            ir_build(IR_ADD, line, 3, tb[0].value, tb[1].value, tb[2].value);
            break;
        case ARITH_SUB:
            ir_build(IR_SUB, line, 3, tb[0].value, tb[1].value, tb[2].value);
            break;
        case ARITH_MULT:
            ir_build(IR_MULT, line, 3, tb[0].value, tb[1].value, tb[2].value);
            break;
        case ARITH_LSHIFT:
            ir_build(IR_LSHIFT, line, 3, tb[0].value, tb[1].value, tb[2].value);
            break;
        case ARITH_RSHIFT:
            ir_build(IR_RSHIFT, line, 3, tb[0].value, tb[1].value, tb[2].value);
            break;
        default:
            parse_errorf(line, "Unknown arithmetic op.");
            return;
    }

    opCount++;
}

void finish_output(int line) {
    word = get_next_token();
    if (word.type != TOK_CONST) {
        parse_errorf(line, "Missing constant in output.");
        return;
    }
    tb[0] = word;

    word = get_next_token();
    if (word.type != TOK_EOL && word.type != TOK_EOF) {
        parse_errorf(line, "Extra token at end of line %s.", token_to_string(word));
        return;
    }

    ir_build(IR_OUTPUT, line, 1, tb[0].value);
    opCount++;
}

void finish_nop(int line) {
    Token word = get_next_token();
    if (word.type != TOK_EOL && word.type != TOK_EOF) {
        parse_errorf(line, "Extra token at end of line %s.", token_to_string(word));
        return;
    }

    ir_build(IR_NOP, line, 0);
    opCount++;
}