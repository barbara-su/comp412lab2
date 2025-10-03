#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#define POOL_SIZE 2000

extern int global_error;

typedef struct IRPool {
    IRNode nodes[POOL_SIZE];  // array of nodes
    int next_free;
    struct IRPool *next;
    struct IRPool *prev;
} IRPool;

static IRPool pool_dummy;  // doubly list of pools
static IRNode node_dummy;  // doubly-linked list of IRNodes

static IRPool *pool_head = &pool_dummy;
static IRNode *node_head = &node_dummy;

void init_pool_list() {
    pool_head->prev = pool_head->next = pool_head;
}

void insert_into_pool_list(struct IRPool *p) {
    struct IRPool *tail = pool_head->prev;
    tail->next = p;
    p->prev = tail;
    p->next = pool_head;
    pool_head->prev = p;
}

void remove_from_pool_list(struct IRPool *p) {
    // check if this removal is redundant
    if ((p->next == p && p->prev == p) ||
        (p->next == NULL && p->prev == NULL)) {
        return;
    }
    struct IRPool *next_p = p->next;
    struct IRPool *prev_p = p->prev;
    prev_p->next = next_p;
    next_p->prev = prev_p;
    p->next = p;
    p->prev = p;
}

void init_node_list() {
    node_head->prev = node_head->next = node_head;
}

void insert_into_node_list(struct IRNode *n) {
    struct IRNode *tail = node_head->prev;
    tail->next = n;
    n->prev = tail;
    n->next = node_head;
    node_head->prev = n;
}

void remove_from_node_list(struct IRNode *n) {
    // check if this removal is redundant
    if ((n->next == n && n->prev == n) ||
        (n->next == NULL && n->prev == NULL)) {
        return;
    }
    struct IRNode *next_n = n->next;
    struct IRNode *prev_n = n->prev;
    prev_n->next = next_n;
    next_n->prev = prev_n;
    n->next = n;
    n->prev = n;
}

// allocate new pool
static IRPool *new_pool(void) {
    IRPool *p = malloc(sizeof(IRPool));
    p->next_free = 0;
    insert_into_pool_list(p);
    return p;
}

IRNode *ir_new_node(IROpcode opcode, int line) {
    IRPool *tail_pool = pool_head->prev;
    if (tail_pool == pool_head || tail_pool->next_free >= POOL_SIZE) {
        tail_pool = new_pool();
    }

    // initialize all fields
    IRNode *n = &tail_pool->nodes[tail_pool->next_free++];
    n->line = line;
    n->opcode = opcode;

    memset(&n->op1, -1, 3 * sizeof(IROperand));
    
    insert_into_node_list(n);

    return n;
}


IRNode *ir_build(IROpcode op, int line, int nops, ...) {
    IRNode *n = ir_new_node(op, line);

    va_list args;
    va_start(args, nops);

    if (nops == 1) n->op1.sr = va_arg(args, int);
    if (nops == 2){
        n->op1.sr = va_arg(args, int);
        n->op3.sr = va_arg(args, int);
    } 
    if (nops == 3){
        n->op1.sr = va_arg(args, int);
        n->op2.sr = va_arg(args, int);
        n->op3.sr = va_arg(args, int);
    } 

    va_end(args);
    
    return n;
}


static void print_operand(IROperand *op, int is_const) {
    if (op->sr != -1) {
        if (is_const) {
            printf("[ val %d ]", op->sr);
        } else {
            printf("[ sr%d ]", op->sr);
        }
    } else {
        printf("[ ]");
    }
}

void ir_print(void) {
    for (IRNode *n = node_head->next; n != node_head; n = n->next) {
        // print opcode name
        switch (n->opcode) {
            case IR_LOAD:
                printf("load\t");
                break;
            case IR_LOADI:
                printf("loadI\t");
                break;
            case IR_STORE:
                printf("store\t");
                break;
            case IR_ADD:
                printf("add\t");
                break;
            case IR_SUB:
                printf("sub\t");
                break;
            case IR_MULT:
                printf("mult\t");
                break;
            case IR_LSHIFT:
                printf("lshift\t");
                break;
            case IR_RSHIFT:
                printf("rshift\t");
                break;
            case IR_OUTPUT:
                printf("output\t");
                break;
            case IR_NOP:
                printf("nop\t");
                break;
        }

        switch (n->opcode) {
            case IR_LOADI:
                print_operand(&n->op1, 1); // constant
                printf(", ");
                print_operand(&n->op2, 0);
                printf(", ");
                print_operand(&n->op3, 0);
                printf("\n");
                break;

            case IR_OUTPUT:
                print_operand(&n->op1, 1); // constant
                printf(", ");
                print_operand(&n->op2, 0);
                printf(", ");
                print_operand(&n->op3, 0);
                printf("\n");
                break;

            default:
                // all other ops: registers
                print_operand(&n->op1, 0);
                printf(", ");
                print_operand(&n->op2, 0);
                printf(", ");
                print_operand(&n->op3, 0);
                printf("\n");
                break;
        }
    }
}

void ir_rename(void){
    // Note: in a, (b) => c; c is definition, and a, b are uses
    int VRName = 0;
    int block_len = 0;
    int maxSR = 0;

    // walk through the blocks to compute block length to compute block_len and maxSR
    for (IRNode *p = node_head -> next; p != node_head; p = p->next){
        block_len++;
        // find max sr
        if (p->op1.sr > maxSR) maxSR = p->op1.sr;
        if (p->op2.sr > maxSR) maxSR = p->op2.sr;
        if (p->op3.sr > maxSR) maxSR = p->op3.sr;
    }

    // Initialize SRToVR and LU
    int *SRToVR = calloc(maxSR + 1, sizeof(int));
    int *LU = calloc(maxSR + 1, sizeof(int));

    int i = 0;
    for (i = 0; i <= maxSR; i++){
        SRToVR[i] = -1;
        LU[i] = INT_MAX;
    }
    int index = block_len - 1;

    // sr for o1, o2, o3
    IROperand op1, op2, op3;

    // Iterate backward through doubly linked list
    for (IRNode *p = node_head->prev; p != node_head; p = p->prev){
        op1 = p->op1;
        op2 = p->op2;
        op3 = p->op3;

        // handle use
        switch (p->opcode)
        {
        // use only r1
        case IR_LOAD:
        case IR_STORE:
            if (SRToVR[op1.sr] == -1) VRName++;
            op1.vr = SRToVR[op1.sr];
            op1.nu = LU[op1.sr];
            SRToVR[op1.sr] = -1;
            LU[op1.sr] = INT_MAX;
            break;
        
        // use both r1 and r2
        case IR_ADD:
        case IR_SUB:
        case IR_MULT:
        case IR_LSHIFT:
        case IR_RSHIFT:
            // use r1
            if (SRToVR[op1.sr] == -1) VRName++;
            op1.vr = SRToVR[op1.sr];
            op1.nu = LU[op1.sr];
            SRToVR[op1.sr] = -1;
            LU[op1.sr] = INT_MAX;
            
            // use r2
            if (SRToVR[op2.sr] == -1) VRName++;
            op2.vr = SRToVR[op2.sr];
            op2.nu = LU[op2.sr];
            SRToVR[op2.sr] = -1;
            LU[op2.sr] = INT_MAX;
            break;
        default:
            break;
        }

        // handle define
        switch (p->opcode)
        {
        // define r2
        case IR_LOAD:
        case IR_LOADI:
        case IR_STORE:
            if (SRToVR[op2.sr] == -1) VRName++;
            op2.vr = SRToVR[op2.sr];
            op2.nu = LU[op2.sr];
            LU[op2.sr] = index;
            break;

        // define r3
        case IR_ADD:
        case IR_SUB:
        case IR_MULT:
        case IR_LSHIFT:
        case IR_RSHIFT:
            if (SRToVR[op3.sr] == -1) VRName++;
            op3.vr = SRToVR[op3.sr];
            op3.nu = LU[op3.sr];
            LU[op3.sr] = index;
            break;

        default:
            break;
        }
        index--;
    }
}

void ir_rename_print(void) {
    IROperand op1, op2, op3;
    for (IRNode *p = node_head->next; p != node_head; p = p->next) {
        op1 = p->op1;
        op2 = p->op2;
        op3 = p->op3;
        switch (p->opcode) {
            case IR_LOADI:
                printf("loadI  %d    => r%d\n", op1.sr, op2.vr);
                break;
            case IR_LOAD:
                printf("load   r%d    => r%d\n", op1.vr, op2.vr);
                break;
            case IR_STORE:
                printf("store  r%d    => r%d\n", op1.vr, op2.vr);
                break;
            case IR_ADD:
                printf("add    r%d, r%d => r%d\n", op1.vr, op2.vr, op3.vr);
                break;
            case IR_SUB:
                printf("sub    r%d, r%d => r%d\n", op1.vr, op2.vr, op3.vr);
                break;
            case IR_MULT:
                printf("mult   r%d, r%d => r%d\n", op1.vr, op2.vr, op3.vr);
                break;
            case IR_LSHIFT:
                printf("lshift r%d, r%d => r%d\n", op1.vr, op2.vr, op3.vr);
                break;
            case IR_RSHIFT:
                printf("rshift r%d, r%d => r%d\n", op1.vr, op2.vr, op3.vr);
                break;
            case IR_OUTPUT:
                printf("output %d\n", op1.sr);
                break;
            default:
                printf("nop\n");
                break;
        }
    }
}
