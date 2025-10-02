#ifndef PARSER_H
#define PARSER_H

#include "scanner.h"

// entry
void parse_program(int *count);

// finish operations
void finish_memop(Token first, int line);
void finish_loadI(int line);
void finish_arithop(Token first, int line);
void finish_output(int line);
void finish_nop(int line);

#endif