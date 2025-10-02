#ifndef SCANNER_H
#define SCANNER_H 

#include <stdio.h>

// 11 categories of token 
typedef enum {
    TOK_MEMOP, // store 0
    TOK_LOADI, // loadI 1
    TOK_ARITHOP, // add, sub, mult, lshift, rshift 2
    TOK_OUTPUT, // output 3
    TOK_NOP, // nop 4
    TOK_CONST, // non-negative integer 5
    TOK_REG, // r followed by integer 6
    TOK_COMMA, // , 7
    TOK_INTO, // => 8
    TOK_EOF, // end of file 9 
    TOK_EOL, // end of line 10
    TOK_ERR, // error 11
} TokenType;

typedef enum {
    ARITH_ADD = 0,
    ARITH_SUB = 1,
    ARITH_MULT = 2,
    ARITH_LSHIFT = 3,
    ARITH_RSHIFT = 4,
} ArithOpCode;

typedef enum {
    LOAD = 0,
    STORE = 1,
} MemOpCode;

// token is category and integer value (optional)
typedef struct{
    TokenType type; //cateogry
    int value; // for CONSTANT / REGESTER
    int line; // line num
} Token;


// Scanner interface, return the next token from input stream
Token get_next_token();

// functions for scanner buffer 
void sb_init(FILE *in);
void sb_free();

#endif
