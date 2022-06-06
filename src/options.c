/*
Copyright 2017-2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "buffer.h"
#include "utils.h"
#include "meta_subst.h"
#include "version.h"
#include "options.h"

enum OPT_ID
    {
    OPT_BAD = -1,
    OPT_OUTPUT = 0,
    OPT_META,
    OPT_VERSION,
    OPT_HELP,
    OPT_ZIP,
    OPT_COUNT
    };

typedef struct opt_map_s
    {
    char const *opt;
    enum OPT_ID id;
    } opt_map_t;

static opt_map_t const Options[]=
    {
    {"-o", OPT_OUTPUT},
    {"-m", OPT_META},
    {"--version", OPT_VERSION},
    {"-h", OPT_HELP},
    {"--help", OPT_HELP},
    {"-z", OPT_ZIP},
    {NULL, OPT_BAD}
    };

static void def_args(opt_args_t *args)
    {
    args->input_file = NULL;
    args->output_file = NULL;
    args->meta_table = NULL;
    args->version = 0;
    args->help = 0;
    args->zip = 0;
    }

static enum OPT_ID opt_match(opt_map_t const *map, char const *opt);
static int add_meta(meta_item_t **meta_table, char const *meta_str);

void print_version(void)
    {
    printf("%s %s\n", APP_NAME, APP_VERSION);
    }

void print_help(void)
    {
    printf("Usage: %s [--version] [-h|--help] [-m meta_tag:meta_value] [-z] [-o output_file.fb2|output_dir] [input_file.md]\n", EXE_NAME);
    }

char *parse_options(opt_args_t *args, char *argv[])
    {
    int file_count = 0;

    def_args(args);

    while(*argv != NULL)
        {
        switch(opt_match(Options, *argv))
            {
        case OPT_OUTPUT:
            if(argv[1] == NULL) return *argv;
            ++argv;
            args->output_file = *argv;
            if(str_ends_with(args->output_file, ".zip")) args->zip = 1;
            break;

        case OPT_META:
            if(argv[1] == NULL) return *argv;
            ++argv;
            if(!add_meta(&args->meta_table, *argv)) return *argv;
            break;

        case OPT_VERSION:
            args->version = 1;
            break;

        case OPT_HELP:
            args->help = 1;
            break;

        case OPT_ZIP:
            args->zip = 1;
            break;

        default:
            if(file_count == 0)
                {
                args->input_file = *argv;
                ++file_count;
                }
            else
                return *argv;
            }

        ++argv;
        }

    return *argv;
    }

static int add_meta(meta_item_t **meta_table, char const *meta_str)
    {
    char const *key_start = meta_str;
    char const *key_end;
    char const *value;
    size_t key_len = 0;
    char *key = NULL;
    meta_item_t *new_item;

    while(isspace(*key_start)) ++key_start;

    key_end = strchr(key_start, ':');

    if(key_end == NULL) return 0;

    value = key_end + 1;

    --key_end;
    while(key_end > key_start && isspace(*key_end)) --key_end;
    ++key_end;

    if(key_end == key_start) return 0;

    while(isspace(*value)) ++value;

    if(*value == '\0') return 0;

    key_len = key_end - key_start;
    key = hoedown_malloc(key_len + 1);
    memcpy(key, key_start, key_len);
    key[key_len] = '\0';

    new_item = meta_item_new(meta_subst(key));
    meta_item_add_value(new_item, (unsigned char *)value);
    meta_table_add(meta_table, new_item);

    free(key);

    return 1;
    }

static enum OPT_ID opt_match(opt_map_t const *map, char const *opt)
    {
    while(NULL != map->opt)
        {
        if(0 == strcmp(map->opt, opt))
            return map->id;

        ++map;
        }

    return OPT_BAD;
    }

