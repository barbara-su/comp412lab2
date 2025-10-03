#include "scanner.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "error.h"

// line buffer
typedef struct {
    char *linebuf;
    size_t bufsize;
    int bufpos;
    int lineno;   // line number
    int linelen;  // lengh of the line
    FILE *in;
} ScannerBuffer;

typedef struct {
    char buf[128];
    int bufpos;
} TokenBuffer;

static ScannerBuffer *sb;
static TokenBuffer tb;
static int tb_recording;

/*
Below are functions for linebuffer.
*/

// initialize scan buffer
void sb_init(FILE *in) {
    sb = malloc(sizeof(ScannerBuffer));

    sb->linebuf = NULL;

    sb->bufsize = 0;
    sb->bufpos = 0;
    sb->lineno = 1;
    sb->linelen = 0;

    sb->in = in;
}

// free scan buffer
void sb_free() {
    if (sb->linebuf) free(sb->linebuf);
    free(sb);
}

// refill scan buffer
static int sb_refill() {
    sb->linebuf = NULL;
    ssize_t nread = getline(&sb->linebuf, &sb->bufsize, sb->in);

    // reaching end of file
    if (nread == -1) {
        return 0;
    }
    sb->bufpos = 0;
    sb->linelen = (int)nread;

    return 1;
}

static inline void do_tb_append(int c) {
    if (c == '\n' || c == '\r') {
        c = ' ';
    }
    tb.buf[tb.bufpos++] = (char)c;
    tb.buf[tb.bufpos] = '\0';
}

// get next char for buffer
static int sb_getc() {
    // if consumed all characters, attempt to refill
    if (sb->bufpos >= sb->linelen) {
        if (!sb_refill(sb)) {
            // printf("consume whole file");
            return EOF;
        }
    }

    // else, spit out next character in line character, increment bufpos
    char c = (char)sb->linebuf[sb->bufpos++];

    if (tb_recording) {
        do_tb_append(c);
    }
    return c;
}

static void start_tb_recording() {
    tb.bufpos = 0;
    tb.buf[0] = '\0';
    tb_recording = 1;
}

/*
functions for scanner logic
*/

// function to create a token (may turn it inline)
static Token make_token(TokenType t, int value, int line) {
    Token tok;
    tok.type = t;
    tok.value = value;
    tok.line = line;
    return tok;
}

// helper function for reporting error
static Token report_error(void) {
    fprintf(stderr, "ERROR %d:\t\"%s\" is not a valid word.\n",
            sb->lineno, tb.buf);

    // debug: print numeric values of buffer contents
    // printf("tb.buf codes: ");
    // for (int i = 0; i < tb.bufpos; i++) {
    //     printf("%d ", (unsigned char)tb.buf[i]);
    // }
    // printf("\n");

    if (sb->bufpos + 1 < sb->linelen) {
        // NUKE whole line (if there are any chars left in this line)
        int c;
        while ((c = sb_getc()) != '\n' && c != EOF) {
            ;
        }
    }
    error_flag = 1;

    return make_token(TOK_EOL, 0, sb->lineno++);
}

Token get_next_token() {
    tb.bufpos = 0;
    tb.buf[0] = '\0';
    tb_recording = 0;

    int c = sb_getc();
    int flag = 0;

    // skip space and tabs
    while (c == ' ' || c == '\t') {
        if (flag == 0) {
            flag = 1;
        }
        c = sb_getc();
    }

    // reset buffer with current character
    if (flag) {
        tb.bufpos = 1;
        tb.buf[0] = c;
        tb.buf[1] = '\0';
    }

    // EOF
    if (c == EOF) {
        return make_token(TOK_EOF, 0, sb->lineno);
    }

    // EOL
    else if (c == '\n') {
        // printf("got U\n");
        return make_token(TOK_EOL, 0, sb->lineno++);
    }

    // EOL case 2
    else if (c == '\r') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();

        if (c == '\n') {
            return make_token(TOK_EOL, 0, sb->lineno++);
        } else {
            if (c != EOF) {
                sb->bufpos--;
            }
            return make_token(TOK_EOL, 0, sb->lineno++);
        }
    }

    // =>

    else if (c == '=') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();
        if (c == '>') {
            return make_token(TOK_INTO, 0, sb->lineno);
        } else {
            return report_error();
        }
    }

    // ,
    else if (c == ',') {
        return make_token(TOK_COMMA, 0, sb->lineno);
    }

    // comment
    else if (c == '/') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();

        if (c == '/') {
            // valid comment, stop recording
            tb_recording = 0;
            c = sb_getc();

            // consumes characters until the end of line
            while (c != '\n' && c != EOF) {
                c = sb_getc();
            }
            return make_token(TOK_EOL, 0, sb->lineno++);
        } else {
            return report_error();
        }
    }

    // starting with s
    else if (c == 's') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();

        // store (opcode)
        if (c == 't') {
            c = sb_getc();
            if (c == 'o') {
                c = sb_getc();
                if (c == 'r') {
                    c = sb_getc();
                    if (c == 'e') {
                        c = sb_getc();
                        if (c == ' ' || c == '\t') {
                            return make_token(TOK_MEMOP, STORE, sb->lineno);
                        } else {
                            return report_error();
                        }
                    } else {
                        return report_error();
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        }

        // sub (opcode)
        else if (c == 'u') {
            start_tb_recording();
            do_tb_append(c);

            c = sb_getc();
            if (c == 'b') {
                c = sb_getc();
                if (c == ' ' || c == '\t') {
                    return make_token(TOK_ARITHOP, ARITH_SUB, sb->lineno);
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        }

        // invalid for starting with s
        else {
            return report_error();
        }
    }

    // starting with l
    else if (c == 'l') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();

        // load / loadI
        if (c == 'o') {
            c = sb_getc();
            if (c == 'a') {
                c = sb_getc();
                if (c == 'd') {
                    c = sb_getc();

                    if (c == 'I') {
                        c = sb_getc();
                        if (c == ' ' || c == '\t') {
                            return make_token(TOK_LOADI, 0, sb->lineno);
                        } else {
                            return report_error();
                        }
                    } else {
                        if (c == ' ' || c == '\t') {
                            return make_token(TOK_MEMOP, LOAD, sb->lineno);
                        } else {
                            return report_error();
                        }
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        }

        // lshift
        else if (c == 's') {
            start_tb_recording();
            do_tb_append(c);

            c = sb_getc();
            if (c == 'h') {
                c = sb_getc();
                if (c == 'i') {
                    c = sb_getc();
                    if (c == 'f') {
                        c = sb_getc();
                        if (c == 't') {
                            c = sb_getc();
                            if (c == ' ' || c == '\t') {
                                return make_token(TOK_ARITHOP, ARITH_LSHIFT, sb->lineno);
                            } else {
                                return report_error();
                            }
                        } else {
                            return report_error();
                        }
                    } else {
                        return report_error();
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        }

        // invalid starting with l
        else {
            return report_error();
        }
    }

    // starting with r
    else if (c == 'r') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();

        // register
        if (c >= '0' && c <= '9') {
            int n = c - '0';
            c = sb_getc();
            while (c >= '0' && c <= '9') {
                n = n * 10 + (c - '0');
                c = sb_getc();
            }

            // decrement buffer pointer
            if (c != EOF) {
                sb->bufpos--;
            }

            return make_token(TOK_REG, n, sb->lineno);
        }

        // rshift (opcode)
        else if (c == 's') {
            start_tb_recording();
            do_tb_append(c);

            c = sb_getc();
            if (c == 'h') {
                c = sb_getc();
                if (c == 'i') {
                    c = sb_getc();
                    if (c == 'f') {
                        c = sb_getc();
                        if (c == 't') {
                            c = sb_getc();
                            if (c == ' ' || c == '\t') {
                                return make_token(TOK_ARITHOP, ARITH_RSHIFT, sb->lineno);
                            } else {
                                return report_error();
                            }
                        } else {
                            return report_error();
                        }
                    } else {
                        return report_error();
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        } else {
            return report_error();
        }
    }

    // add (opcode)
    else if (c == 'a') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();
        if (c == 'd') {
            c = sb_getc();
            if (c == 'd') {
                c = sb_getc();
                if (c == ' ' || c == '\t') {
                    return make_token(TOK_ARITHOP, ARITH_ADD, sb->lineno);
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        } else {
            return report_error();
        }
    }

    // mult (opcode)
    else if (c == 'm') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();
        if (c == 'u') {
            c = sb_getc();
            if (c == 'l') {
                c = sb_getc();
                if (c == 't') {
                    c = sb_getc();
                    if (c == ' ' || c == '\t') {
                        return make_token(TOK_ARITHOP, ARITH_MULT, sb->lineno);
                    } else {
                        return report_error();
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        } else {
            return report_error();
        }
    }

    // nop (opcode, no need for space)
    else if (c == 'n') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();
        if (c == 'o') {
            c = sb_getc();
            if (c == 'p') {
                return make_token(TOK_NOP, 0, sb->lineno);
            } else {
                return report_error();
            }
        } else {
            return report_error();
        }
    }

    // output (opcode)
    else if (c == 'o') {
        start_tb_recording();
        do_tb_append(c);

        c = sb_getc();
        if (c == 'u') {
            c = sb_getc();
            if (c == 't') {
                c = sb_getc();
                if (c == 'p') {
                    c = sb_getc();
                    if (c == 'u') {
                        c = sb_getc();
                        if (c == 't') {
                            c = sb_getc();
                            if (c == ' ' || c == '\t') {
                                return make_token(TOK_OUTPUT, 0, sb->lineno);
                            } else {
                                return report_error();
                            }
                        } else {
                            return report_error();
                        }
                    } else {
                        return report_error();
                    }
                } else {
                    return report_error();
                }
            } else {
                return report_error();
            }
        } else {
            return report_error();
        }
    }

    // store constant into n (an int will be parse anyway)
    else if (c >= '0' && c <= '9') {
        int n = c - '0';  // convert char to int

        c = sb_getc();
        while (c >= '0' && c <= '9') {
            n = n * 10 + (c - '0');
            c = sb_getc();
        }

        // decrement buffer pointer
        if (c != EOF) {
            sb->bufpos--;
        }

        return make_token(TOK_CONST, n, sb->lineno);
    }

    else {
        do_tb_append(c);
        return report_error();
    }
}