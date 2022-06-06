/*
Copyright 2017 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <ctype.h>
#include "buffer.h"
#include "uthash.h"
#include "meta.h"

typedef struct meta_string_list meta_string_list_t;

struct meta_string_list
    {
    unsigned char *data;
    meta_string_list_t *next;
    };

struct meta_item
    {
    char *key;
    meta_string_list_t *values;
    meta_item_t *next;
    UT_hash_handle hh;
    };

static meta_string_list_t UnknownString =
    {
    (unsigned char *)"unknown",
    NULL
    };

static meta_item_t const UnknownItem =
    {
    "", /* key */
    &UnknownString,
    NULL, /* next */
    {0} /* hh */
    };

static meta_string_list_t *meta_string_new_mem(void const *value, size_t value_len)
    {
    meta_string_list_t *values = hoedown_malloc(sizeof(*values) + value_len + 1);

    values->data = (unsigned char *)(values + 1);
    memcpy(values->data, value, value_len);
    values->data[value_len] = '\0';
    values->next = NULL;

    return values;
    }

static meta_string_list_t *meta_string_new(unsigned char const *value)
    {
    return meta_string_new_mem(value, strlen((char *)value));
    }

static void meta_string_del(meta_string_list_t *values)
    {
    meta_string_list_t *next;

    while(values != NULL)
        {
        next = values->next;
        free(values);
        values = next;
        }
    }

static unsigned meta_string_add_mem(meta_string_list_t *values, void const *value, size_t value_size)
    {
    unsigned n = 1;

    while(values->next != NULL)
        {
        values = values->next;
        ++n;
        }

    values->next = meta_string_new_mem(value, value_size);
    ++n;

    return n;
    }

static unsigned meta_string_add(meta_string_list_t *values, unsigned char const *value)
    {
    return meta_string_add_mem(values, value, strlen((char *)value));
    }

static unsigned meta_string_count(meta_string_list_t const *values)
    {
    unsigned n = 0;

    while(values != NULL)
        {
        values = values->next;
        ++n;
        }

    return n;
    }

static unsigned char *meta_string_value(meta_string_list_t const *values, unsigned i)
    {
    while(values != NULL && i > 0)
        {
        values = values->next;
        --i;
        }

    return values == NULL || values->data == NULL ? NULL : values->data;
    }

static meta_string_list_t *meta_string_split_str(unsigned char const *str)
    {
    meta_string_list_t *parts = NULL;
    unsigned char const *begin;
    unsigned char const *end;

    if(str == NULL)
        return parts;

    begin = str;

    do
        {
        while(isspace(*begin))
            ++begin;

        if(*begin == '\'' || *begin == '"')
            {
            end = begin + 1;

            while(*end != '\0' && *end != *begin)
                ++end;

            ++begin;
            }
        else
            {
            end = begin;

            while(*end != '\0' && !isspace(*end))
                ++end;
            }

        if(end > begin)
            {
            if(parts == NULL)
                parts = meta_string_new_mem(begin, end - begin);
            else
                meta_string_add_mem(parts, begin, end - begin);
            }

        begin = end + 1;
        }
    while(*end != '\0');

    return parts;
    }

meta_item_t *meta_item_new_mem(void const *key, size_t key_len)
    {
    meta_item_t *item = hoedown_calloc(1, sizeof(*item));

    item->key = hoedown_malloc(key_len + 1);
    memcpy(item->key, key, key_len);
    item->key[key_len] = '\0';

    return item;
    }

meta_item_t *meta_item_new(char const *key)
    {
    return meta_item_new_mem(key, strlen(key));
    }

void meta_item_del(meta_item_t *item)
    {
    meta_item_t *next;

    while(item != NULL)
        {
        next = item->next;
        meta_string_del(item->values);
        free(item->key);
        free(item);
        item = next;
        }
    }

unsigned meta_item_add_value(meta_item_t *item, unsigned char const *value)
    {
    unsigned n = 0;

    if(item->values == NULL)
        {
        item->values = meta_string_new(value);
        ++n;
        }
    else
        n = meta_string_add(item->values, value);

    return n;
    }

char *meta_item_key(meta_item_t const *item)
    {
    return item->key;
    }

unsigned meta_item_count(meta_item_t const *item)
    {
    unsigned n = 0;

    while(item != NULL)
        {
        item = item->next;
        ++n;
        }

    return n;
    }

meta_item_t *meta_item_get(meta_item_t *item, unsigned i)
    {
    while(i > 0)
        {
        item = item->next;
        --i;
        }

    return item;
    }

unsigned meta_item_count_values(meta_item_t const *item)
    {
    return meta_string_count(item->values);
    }

meta_item_t *meta_item_split_value(meta_item_t const *item, unsigned i)
    {
    meta_item_t *parts = NULL;
    meta_string_list_t *values = meta_string_split_str(meta_string_value(item->values, i));

    if(values != NULL)
        {
        parts = meta_item_new(item->key);
        parts->values = values;
        }

    return parts;
    }

unsigned char const *meta_item_value(meta_item_t const *item, unsigned i)
    {
    unsigned char const *str = meta_string_value(item->values, i);

    return str == NULL ? (unsigned char *)"" : str;
    }

void meta_item_add_item(meta_item_t *item, meta_item_t *next)
    {
    while(item->next != NULL)
        item = item->next;

    item->next = next;
    }

meta_item_t const *meta_item_get_unknown(void)
    {
    return &UnknownItem;
    }

meta_item_t *meta_item_next(meta_item_t const *item)
    {
    return item->next;
    }

void meta_table_add(meta_item_t **table, meta_item_t *item)
    {
    HASH_ADD_KEYPTR(hh, *table, item->key, strlen(item->key), item);
    }

meta_item_t *meta_table_get(meta_item_t **table, char const *key)
    {
    meta_item_t *item;

    HASH_FIND(hh, *table, key, strlen(key), item);

    return item;
    }

void meta_table_clear(meta_item_t **table)
    {
    meta_item_t *item, *tmp;

    HASH_ITER(hh, *table, item, tmp)
        {
        HASH_DEL(*table, item);
        meta_item_del(item);
        }
  }

meta_item_t *meta_table_next(meta_item_t **table)
    {
    if(*table)
        return (*table)->hh.next;
    else
        return NULL;
    }

