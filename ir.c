#include "ir.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int vrname = 0;

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
    if (nops == 2) {
        n->op1.sr = va_arg(args, int);
        n->op3.sr = va_arg(args, int);
    }
    if (nops == 3) {
        n->op1.sr = va_arg(args, int);
        n->op2.sr = va_arg(args, int);
        n->op3.sr = va_arg(args, int);
    }

    va_end(args);

    return n;
}


static void print_operand(IROperand *op, int is_const) {
    if (op->sr == -1) {
        printf("[ ]");
        return;
    }
    if (is_const) {
        printf("[ val %d ]", op->sr);
    } else {
        if (op->nu == INT_MAX) {
            printf("[ sr%d vr%d nu=-1 ]", op->sr, op->vr);
        } else {
            printf("[ sr%d vr%d nu=%d ]", op->sr, op->vr, op->nu);
        }
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
                print_operand(&n->op1, 1);  // constant
                printf(", ");
                print_operand(&n->op2, 0);
                printf(", ");
                print_operand(&n->op3, 0);
                printf("\n");
                break;

            case IR_OUTPUT:
                print_operand(&n->op1, 1);  // constant
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

static inline void tag_def(IROperand *o, int *SRToVR, int *LU) {
    if (SRToVR[o->sr] < 0) {
        SRToVR[o->sr] = vrname++;
    }

    o->vr = SRToVR[o->sr];
    o->nu = LU[o->sr];

    SRToVR[o->sr] = -1;
    LU[o->sr] = INT_MAX;
}

static inline void tag_use(IROperand *o, int *SRToVR, int *LU) {
    if (SRToVR[o->sr] < 0) { // unused def
        SRToVR[o->sr] = vrname++;
    }
    o->vr = SRToVR[o->sr];
    o->nu = LU[o->sr];
}

void ir_rename(int *maxlive_out) {
    // compute block length and max_sr_seen, through iterating the irnodes
    int block_len = 0, max_sr = -1;
    for (IRNode *p = node_head->next; p != node_head; p = p->next) {
        block_len++;
        if (p->op1.sr > max_sr) max_sr = p->op1.sr;
        if (p->op2.sr > max_sr) max_sr = p->op2.sr;
        if (p->op3.sr > max_sr) max_sr = p->op3.sr;
    }
    int nSR = (max_sr >= 0 ? max_sr + 1 : 1);  // sr is 0 index

    // create tables in algorithm, and initaalize field
    int *SRToVR = calloc(nSR, sizeof(int));
    int *LU = calloc(nSR, sizeof(int));

    for (int i = 0; i < nSR; i++) {
        SRToVR[i] = -1;  // invalid
        LU[i] = INT_MAX;
    }

    vrname = 0;
    int index = block_len - 1;
    int maxlive = 0;
    int live = 0;

    // Walk from bottom to top
    for (IRNode *p = node_head->prev; p != node_head; p = p->prev) {
        // for each operand O that OP defines
        switch (p->opcode) {
            case IR_LOAD:
            case IR_LOADI:
            case IR_ADD:
            case IR_SUB:
            case IR_MULT:
            case IR_LSHIFT:
            case IR_RSHIFT:
                tag_def(&p->op3, SRToVR, LU);
                break;
            default:
                break;
        }
        // for each operand O that OP uses
        switch (p->opcode) {
            case IR_LOAD:
                tag_use(&p->op1, SRToVR, LU);
                break;
            case IR_STORE:
                tag_use(&p->op1, SRToVR, LU);
                tag_use(&p->op3, SRToVR, LU);
                break;
            case IR_ADD:
            case IR_SUB:
            case IR_MULT:
            case IR_LSHIFT:
            case IR_RSHIFT:
                tag_use(&p->op1, SRToVR, LU);
                tag_use(&p->op2, SRToVR, LU);
                break;
            case IR_OUTPUT:
                tag_use(&p->op1, SRToVR, LU);
                break;
            default:
                break;
        }

        switch (p->opcode) {
            case IR_LOAD:
                LU[p->op1.sr] = index;
                break;
            case IR_STORE:
                LU[p->op1.sr] = index;
                LU[p->op3.sr] = index;
                break;
            case IR_ADD:
            case IR_SUB:
            case IR_MULT:
            case IR_LSHIFT:
            case IR_RSHIFT:
                LU[p->op1.sr] = index;
                LU[p->op2.sr] = index;
                break;
            case IR_OUTPUT:
                LU[p->op1.sr] = index;
                break;
            default:
                break;
        }
        index--;

        // track maxlive (# of sr currently mapped to vrs)
        live = 0;
        for (int i = 0; i < nSR; i++)
            if (SRToVR[i] >= 0) live++;
        if (live > maxlive) maxlive = live;
    }
    // update maxlive and implicitely return
    if (maxlive_out) *maxlive_out = maxlive;

    free(SRToVR);
    free(LU);
}