#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "ir.h"
#include "parser.h"
#include "scanner.h"

int error_flag = 0;

static void print_usage() {
    printf("COMP 412, Reference Allocator (lab 2)\n");
    printf("Command Syntax:\n");
    printf("    412alloc k filename [-x] [-h]\n\n");

    printf("Required arguments:\n");
    printf("    filename  is the pathname (absolute or relative) to the input file\n\n");

    printf("Optional flags:\n");
    printf("\t-h\t prints this message\n");
    printf("\t-x\t runs renamer and prints renamed IR code\n");
}

int main(int argc, char* argv[]) {
    int opt;
    int hflag = 0, xflag = 0;

    opterr = 0;

    while ((opt = getopt(argc, argv, "hx")) != -1) {
        switch (opt) {
            case 'h':
                hflag = 1;
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

    if (hflag) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (!xflag) {
        fprintf(stderr, "ERROR: Missing required -x flag\n");
        print_usage();
        return EXIT_FAILURE;
    }

    if (optind >= argc) {
        fprintf(stderr, "ERROR: Missing filename\n");
        print_usage();
        return EXIT_FAILURE;
    }

    const char* filename = argv[optind];
    FILE* f_in = fopen(filename, "r");
    if (!f_in) {
        fprintf(stderr, "ERROR: Could not open file '%s'\n", filename);
        return EXIT_FAILURE;
    }

    // initialize scanner, IR pools
    sb_init(f_in);
    init_pool_list();
    init_node_list();

    int count = 0;
    parse_program(&count);

    if (!error_flag) {
        ir_rename();
        ir_rename_print();
    } else {
        fprintf(stderr, "\nDue to syntax error(s), run terminates.\n");
    }

    sb_free();
    fclose(f_in);
    return EXIT_SUCCESS;
}
