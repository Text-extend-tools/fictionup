/*
Copyright 2017-2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include "buffer.h"
#include "document.h"
#include "fb2.h"
#include "options.h"
#include "utils.h"
#include "meta_subst.h"
#include "parse_yaml.h"
#include "write_zip.h"

#define BUF_UNIT 65536
#define MAX_NESTING 16
#define PATH_UNIT 64

static hoedown_buffer *make_out_name(char const *in_name, char const *dir);
static hoedown_buffer *make_data_name(hoedown_buffer const *out_name);
static hoedown_buffer *convert_to_fb2(
    hoedown_buffer const *markdown,
    hoedown_buffer const *data_dir,
    meta_item_t **meta_table
    );
static meta_item_t *load_def_meta(void);

int main(int argc, char *argv[])
    {
    opt_args_t args;
    char *ret;
    FILE *in = stdin;
    FILE *out = stdout;
    hoedown_buffer *data_dir = NULL;
    hoedown_buffer *out_name = NULL;
    hoedown_buffer *data_name = NULL;
    hoedown_buffer *markdown;
    hoedown_buffer *fb2;

    /* Load data files */
    meta_subst_init();
    meta_subst_load_system(argv[0]);
    meta_subst_load_user();

    /* Parse arguments */
    (void)argc;

    ret = parse_options(&args, argv + 1);
    if(ret != NULL)
        {
        fprintf(stderr, "ERROR: Bad option: %s\n", ret);
        return EXIT_FAILURE;
        }

    if(args.help)
        {
        print_version();
        print_help();
        return EXIT_SUCCESS;
        }

    if(args.version)
        {
        print_version();
        return EXIT_SUCCESS;
        }

    /* Open input file */
    if(args.input_file != NULL && strcmp(args.input_file, "-") == 0)
        args.input_file = NULL;

    if(args.input_file != NULL)
        {
        char const *found;

        in = fopen(args.input_file, "r");
        if(in == NULL)
            {
            fprintf(stderr, "ERROR: Can't open input file: %s\n", args.input_file);
            perror("ERROR");
            return EXIT_FAILURE;
            }

        /* Extract data directory path */
        found = str_last_path_part(args.input_file);

        if(found != args.input_file)
            {
            data_dir = hoedown_buffer_new(PATH_UNIT);
            hoedown_buffer_set(data_dir, (uint8_t const *)args.input_file, found - args.input_file);
            }
        }

    /* Prepare output file name */
    if(args.output_file != NULL && *args.output_file == '\0')
        {
        /* Output name is empty string, use input file directory for output */
        if(args.input_file != NULL)
            out_name = make_out_name(args.input_file, NULL);
        else
            out_name = make_out_name("-", NULL);
        }

    else if(args.output_file != NULL && is_directory(args.output_file))
        {
        /* Output name is directory, use it for output */
        if(args.input_file != NULL)
            out_name = make_out_name(args.input_file, args.output_file);
        else
            out_name = make_out_name("-", args.output_file);
        }

    else if(args.output_file == NULL && args.input_file != NULL)
        {
        /* No output name, use current directory for output */
        out_name = make_out_name(args.input_file, ".");
        }

    if(out_name != NULL)
        {
        /* out_name was constructed */
        if(args.zip)
            {
            data_name = make_data_name(out_name);
            hoedown_buffer_puts(out_name, ".zip");
            }

        args.output_file = hoedown_buffer_cstr(out_name);
        }

    if(args.zip && data_name == NULL)
        {
        if(args.input_file != NULL)
            {
            /* Make data_name from input file name */
            data_name = make_out_name(args.input_file, "");
            }
        else
            {
            /* Use default data_name */
            data_name = hoedown_buffer_new(PATH_UNIT);
            hoedown_buffer_puts(data_name, "fictionup.fb2");
            }
        }

    if(args.output_file != NULL && strcmp(args.output_file, "-") == 0)
        args.output_file = NULL;

    /* Prepare output file */
    if(args.output_file != NULL)
        {
        out = fopen(args.output_file, "wb");
        if(out == NULL)
            {
            fprintf(stderr, "ERROR: Can't create output file: %s\n", args.output_file);
            perror("ERROR");
            return EXIT_FAILURE;
            }
        }

    /* DO CONVERSION */
    markdown = read_file(in, 1);
    fb2 = convert_to_fb2(markdown, data_dir, &args.meta_table);

    if(!args.zip)
        {
        /* Write FB2 */
        if(fwrite(fb2->data, 1, fb2->size, out) != fb2->size || fflush(out) != 0)
            {
            fprintf(stderr, "ERROR: Write failed!\n");
            perror("ERROR");
            return EXIT_FAILURE;
            }
        }
    else
        {
        /* Write ZIP */
        if(!write_zip(out, hoedown_buffer_cstr(data_name), fb2->data, fb2->size))
            return EXIT_FAILURE;
        }

    /* Cleanup */
    if(out != stdout) fclose(out);
    if(in != stdin) fclose(in);
    hoedown_buffer_free(fb2);
    hoedown_buffer_free(markdown);
    hoedown_buffer_free(data_name);
    hoedown_buffer_free(out_name);
    hoedown_buffer_free(data_dir);
    meta_table_clear(&args.meta_table);
    meta_subst_fini();

    return EXIT_SUCCESS;
    }

static hoedown_buffer *make_out_name(char const *in_name, char const *dir)
    {
    hoedown_buffer *out_name = hoedown_buffer_new(PATH_UNIT);
    char const *name, *ext;

    if(dir != NULL)
        {
        hoedown_buffer_puts(out_name, dir);
        add_path_separator(out_name);
        name = str_last_path_part(in_name);
        }
    else
        name = in_name;

    ext = str_ends_with(name, ".md");

    if(ext == NULL)
        ext = str_ends_with(name, ".markdown");

    if(ext == NULL)
        ext = name + strlen(name);

    hoedown_buffer_put(out_name, (uint8_t const *)name, ext - name);
    hoedown_buffer_puts(out_name, ".fb2");

    return out_name;
    }

static hoedown_buffer *make_data_name(hoedown_buffer const *out_name)
    {
    hoedown_buffer *data_name = hoedown_buffer_new(PATH_UNIT);
    uint8_t const *fn = mem_last_path_part(out_name->data, out_name->size);

    hoedown_buffer_put(data_name, fn, out_name->size - (fn - out_name->data));

    return data_name;
    }

static hoedown_buffer *convert_to_fb2(
    hoedown_buffer const *markdown,
    hoedown_buffer const *data_dir,
    meta_item_t **meta_table
    )
    {
    hoedown_renderer *fb2;
    hoedown_document *doc;
    hoedown_buffer *body;
    hoedown_buffer *header;
    hoedown_buffer *footer;
    meta_item_t *def_table = NULL;

    body = hoedown_buffer_new(BUF_UNIT);
    fb2 = fb2_renderer_new(NULL, data_dir);
    doc = hoedown_document_new(fb2, fb2_get_extensions(), MAX_NESTING);

    hoedown_document_render(doc, body, markdown->data, markdown->size);
    def_table = load_def_meta();
    header = fb2_make_header(fb2, meta_table, def_table);
    footer = fb2_make_footer(fb2);

    hoedown_buffer_put(header, body->data, body->size);
    hoedown_buffer_put(header, footer->data, footer->size);

    meta_table_clear(&def_table);
    hoedown_buffer_free(footer);
    hoedown_document_free(doc);
    fb2_renderer_delete(fb2);
    hoedown_buffer_free(body);

    return header;
    }

static meta_item_t *load_def_meta(void)
    {
    meta_item_t *def = NULL;
    hoedown_buffer *path = get_user_data_path();
    FILE *in = NULL;

    if(path == NULL)
        return def;

    append_path_str(path, "default.yaml");
    in = fopen(hoedown_buffer_cstr(path), "r");

    if(in != NULL)
        {
        hoedown_buffer *yaml = read_file(in, 0);

        if(yaml != NULL)
            {
            if(!parse_yaml(&def, yaml->data, yaml->size, 1))
                fprintf(stderr, "WARNING: Bad default meta information: %s\n",
                    hoedown_buffer_cstr(path));
            }

        hoedown_buffer_free(yaml);
        fclose(in);
        }

    hoedown_buffer_free(path);

    return def;
    }

