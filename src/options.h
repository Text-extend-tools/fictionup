#pragma once
#include "meta.h"

typedef struct opt_args_s
    {
    char const *input_file;
    char const *output_file;
    meta_item_t *meta_table;
    int version;
    int help;
    int zip;
    } opt_args_t;

char *parse_options(opt_args_t *args, char *argv[]);
void print_version(void);
void print_help(void);

