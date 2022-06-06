#pragma once

typedef struct meta_item meta_item_t;

meta_item_t *meta_item_new(char const *key);
meta_item_t *meta_item_new_mem(void const *key, size_t key_len);
void meta_item_del(meta_item_t *item);
unsigned meta_item_add_value(meta_item_t *item, unsigned char const *value);
char *meta_item_key(meta_item_t const *item);
unsigned meta_item_count(meta_item_t const *item);
meta_item_t *meta_item_get(meta_item_t *item, unsigned i);
unsigned meta_item_count_values(meta_item_t const *item);
unsigned char const *meta_item_value(meta_item_t const *item, unsigned i);
meta_item_t *meta_item_split_value(meta_item_t const *item, unsigned i);
void meta_item_add_item(meta_item_t *item, meta_item_t *next);
meta_item_t *meta_item_next(meta_item_t const *item);
meta_item_t const *meta_item_get_unknown(void);

void meta_table_add(meta_item_t **table, meta_item_t *item);
meta_item_t *meta_table_get(meta_item_t **table, char const *key);
void meta_table_clear(meta_item_t **table);
meta_item_t *meta_table_next(meta_item_t **table);
