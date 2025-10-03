#ifndef IR_H
#define IR_H

#include "scanner.h"

typedef enum {
    IR_LOAD,
    IR_LOADI,
    IR_STORE,
    IR_ADD,
    IR_SUB,
    IR_MULT,
    IR_LSHIFT,
    IR_RSHIFT,
    IR_OUTPUT,
    IR_NOP
} IROpcode;

typedef struct {
    int sr;
    int vr;
    int pr;
    int nu;
} IROperand;

typedef struct IRNode {
    int line; // source line number
    IROpcode opcode; // operation type
    IROperand op1, op2, op3; // up to 3 operands
    struct IRNode *prev;
    struct IRNode *next;
} IRNode;

// init functions
void init_pool_list();
void init_node_list();

// builder function for each ir code
IRNode *ir_build(IROpcode op, int line, int nops, ...);

// print function
void ir_print(void);

// renames
void ir_rename(void);
void ir_rename_print(void);

#endif
