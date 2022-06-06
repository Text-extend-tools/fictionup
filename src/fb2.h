#pragma once

#include "document.h"
#include "meta.h"

#define FB2_LIST_DEPTH 10

typedef struct fb2_config
    {
    char const *list_indent[FB2_LIST_DEPTH];
    char const *list_spacing[FB2_LIST_DEPTH];
    char const *list_marker[FB2_LIST_DEPTH];
    char const *list_delimiter[FB2_LIST_DEPTH];
    } fb2_config_t;

hoedown_renderer *fb2_renderer_new(fb2_config_t const *config, hoedown_buffer const *data_dir);
void fb2_renderer_delete(hoedown_renderer *r);
int fb2_get_extensions(void);
hoedown_buffer *fb2_get_yaml(hoedown_renderer *r);
hoedown_buffer *fb2_make_header(hoedown_renderer *r, meta_item_t **meta, meta_item_t *def);
hoedown_buffer *fb2_make_footer(hoedown_renderer *r);
