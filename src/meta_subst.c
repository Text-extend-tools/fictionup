/*
Copyright 2017-2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include <stdio.h>
#include "uthash.h"
#include "buffer.h"
#include "utils.h"
#include "parse_meta_lang.h"
#include "meta_subst.h"

typedef struct meta_subst_s
    {
    char const *key;
    char const *value;
    char const *lang;
    void *mem;
    UT_hash_handle hh;
    } meta_subst_t;

typedef struct meta_lang_s
    {
    char *key;
    unsigned count;
    UT_hash_handle hh;
    } meta_lang_t;

static meta_subst_t KnownTagsList[] =
    {
    { "fb2-author", "fb2-author", NULL, NULL, {0} },
    { "fb2-date", "fb2-date", NULL, NULL, {0} },
    { "fb2-id", "fb2-id", NULL, NULL, {0} },
    { "fb2-version", "fb2-version", NULL, NULL, {0} },
    /* EN */
    { "genre", "genre", "en", NULL, {0} },
    { "Genre", "genre", "en", NULL, {0} },
    { "GENRE", "genre", "en", NULL, {0} },
    { "author", "author", "en", NULL, {0} },
    { "Author", "author", "en", NULL, {0} },
    { "AUTHOR", "author", "en", NULL, {0} },
    { "title", "title", "en", NULL, {0} },
    { "Title", "title", "en", NULL, {0} },
    { "TITLE", "title", "en", NULL, {0} },
    { "lang", "lang", "en", NULL, {0} },
    { "Lang", "lang", "en", NULL, {0} },
    { "LANG", "lang", "en", NULL, {0} },
    { "language", "lang", "en", NULL, {0} },
    { "Language", "lang", "en", NULL, {0} },
    { "LANGUAGE", "lang", "en", NULL, {0} },
    { "cover", "cover", "en", NULL, {0} },
    { "Cover", "cover", "en", NULL, {0} },
    { "COVER", "cover", "en", NULL, {0} },
    { "date", "date", "en", NULL, {0} },
    { "Date", "date", "en", NULL, {0} },
    { "DATE", "date", "en", NULL, {0} },
    { "year", "date", "en", NULL, {0} },
    { "Year", "date", "en", NULL, {0} },
    { "YEAR", "date", "en", NULL, {0} },
    { "annotation", "annotation", "en", NULL, {0} },
    { "Annotation", "annotation", "en", NULL, {0} },
    { "ANNOTATION", "annotation", "en", NULL, {0} },
    { "series", "series", "en", NULL, {0} },
    { "Series", "series", "en", NULL, {0} },
    { "SERIES", "series", "en", NULL, {0} },
    /* RU */
    { "жанр", "genre", "ru", NULL, {0} },
    { "Жанр", "genre", "ru", NULL, {0} },
    { "ЖАНР", "genre", "ru", NULL, {0} },
    { "автор", "author", "ru", NULL, {0} },
    { "Автор", "author", "ru", NULL, {0} },
    { "АВТОР", "author", "ru", NULL, {0} },
    { "название", "title", "ru", NULL, {0} },
    { "Название", "title", "ru", NULL, {0} },
    { "НАЗВАНИЕ", "title", "ru", NULL, {0} },
    { "язык", "lang", "ru", NULL, {0} },
    { "Язык", "lang", "ru", NULL, {0} },
    { "ЯЗЫК", "lang", "ru", NULL, {0} },
    { "обложка", "cover", "ru", NULL, {0} },
    { "Обложка", "cover", "ru", NULL, {0} },
    { "ОБЛОЖКА", "cover", "ru", NULL, {0} },
    { "дата", "date", "ru", NULL, {0} },
    { "Дата", "date", "ru", NULL, {0} },
    { "ДАТА", "date", "ru", NULL, {0} },
    { "год", "date", "ru", NULL, {0} },
    { "Год", "date", "ru", NULL, {0} },
    { "ГОД", "date", "ru", NULL, {0} },
    { "аннотация", "annotation", "ru", NULL, {0} },
    { "Аннотация", "annotation", "ru", NULL, {0} },
    { "АННОТАЦИЯ", "annotation", "ru", NULL, {0} },
    { "серия", "series", "ru", NULL, {0} },
    { "Серия", "series", "ru", NULL, {0} },
    { "СЕРИЯ", "series", "ru", NULL, {0} },
    };

static meta_subst_t *KnownTags = NULL;
static meta_lang_t *LangCounters = NULL;

static void load_lang_dir(hoedown_buffer const *path);
static void load_lang_file(char const *file_name);
static void meta_lang_add(meta_lang_t **table, char const *lang);
static char *meta_lang_search_max(meta_lang_t **table);
static void meta_lang_clear(meta_lang_t **table);

void meta_subst_init(void)
    {
    unsigned i;

    for(i = 0; i < sizeof(KnownTagsList) / sizeof(*KnownTagsList); ++i)
        HASH_ADD_STR(KnownTags, key, (KnownTagsList + i));
    }

void meta_subst_add(char const *key, char const *value, char const *lang)
    {
    size_t key_size = strlen(key) + 1;
    size_t value_size = strlen(value) + 1;
    size_t lang_size = strlen(lang) + 1;
    meta_subst_t *add;
    meta_subst_t *del;
    char *mem;

    HASH_FIND_STR(KnownTags, key, del);

    if(del)
        {
        HASH_DEL(KnownTags, del);

        if(del->mem)
            free(del->mem);
        }

    add = hoedown_calloc(1, sizeof(*add) + key_size + value_size + lang_size);

    add->mem = add;

    mem = add->mem;
    mem += sizeof(*add);

    memcpy(mem, key, key_size);
    add->key = mem;
    mem += key_size;

    memcpy(mem, value, value_size);
    add->value = mem;
    mem += value_size;

    memcpy(mem, lang, lang_size);
    add->lang = mem;

    HASH_ADD_STR(KnownTags, key, add);
    }

void meta_subst_fini(void)
    {
    meta_subst_t *del, *tmp;

    HASH_ITER(hh, KnownTags, del, tmp)
        {
        if(del->mem)
            {
            HASH_DEL(KnownTags, del);
            free(del->mem);
            }
        }

    HASH_CLEAR(hh, KnownTags);

    meta_lang_clear(&LangCounters);
    }

static meta_subst_t const *meta_subst_search(char const *key)
    {
    meta_subst_t *subst;
    unsigned i;

    if(KnownTags)
        {
        HASH_FIND_STR(KnownTags, key, subst);
        return subst;
        }

    for(i = 0; i < sizeof(KnownTagsList) / sizeof(*KnownTagsList); ++i)
        {
        if(strcmp(KnownTagsList[i].key, key) == 0)
            return KnownTagsList + i;
        }

    return NULL;
    }

static char const *meta_subst_replace(char const **key)
    {
    meta_subst_t const *subst = meta_subst_search(*key);

    if(subst == NULL)
        {
        fprintf(stderr, "WARNING: Unknown meta information tag: %s\n", *key);
        return NULL;
        }
    else
        *key = subst->value;

    return subst->lang;
    }

char const *meta_subst_def(char const *key)
    {
    meta_subst_replace(&key);

    return key;
    }

char const *meta_subst(char const *key)
    {
    char const *lang = meta_subst_replace(&key);

    if(lang != NULL && *lang != '\0')
        meta_lang_add(&LangCounters, lang);

    return key;
    }

char const *meta_subst_get_lang(void)
    {
    return meta_lang_search_max(&LangCounters);
    }

void meta_subst_load_system(char const *argv0)
    {
    hoedown_buffer *path = get_data_path(argv0);

    append_path_str(path, "meta-lang");
    load_lang_dir(path);
    hoedown_buffer_free(path);
    }

void meta_subst_load_user(void)
    {
    hoedown_buffer *path = get_user_data_path();

    if(path == NULL)
        return;

    append_path_str(path, "meta-lang");
    load_lang_dir(path);
    hoedown_buffer_free(path);
    }

static void load_lang_dir(hoedown_buffer const *path)
    {
    glob_t list;
    size_t i;

    list_files(&list, path, "*.yaml");

    for(i = 0; i < list.gl_pathc; ++i)
        load_lang_file(list.gl_pathv[i]);

    list_files_free(&list);
    }

static void load_lang_file(char const *file_name)
    {
    FILE *in = NULL;
    hoedown_buffer *lang = NULL;

    in = fopen(file_name, "r");
    if(in == NULL)
        {
        fprintf(stderr, "WARNING: Can't open language file: %s\n", file_name);
        perror("WARNING");
        return;
        }

    lang = read_file(in, 0);

    if(lang != 0)
        {
        if(!parse_meta_lang(lang->data, lang->size))
            fprintf(stderr, "WARNING: Bad language file: %s\n", file_name);
        }

    hoedown_buffer_free(lang);
    fclose(in);
    }

static meta_lang_t *meta_lang_new(char const *lang)
    {
    size_t lang_len = strlen(lang);
    meta_lang_t *item = hoedown_malloc(sizeof(*item) + lang_len + 1);

    item->key = (char *)(item + 1);
    memcpy(item->key, lang, lang_len + 1);
    item->count = 1;

    return item;
    }

static void meta_lang_add(meta_lang_t **table, char const *lang)
    {
    meta_lang_t *item;

    HASH_FIND(hh, *table, lang, strlen(lang), item);

    if(item)
        ++item->count;
    else
        {
        item = meta_lang_new(lang);
        HASH_ADD_KEYPTR(hh, *table, item->key, strlen(item->key), item);
        }
    }

static char *meta_lang_search_max(meta_lang_t **table)
    {
    meta_lang_t *max_item = *table;
    meta_lang_t *item;

    if(max_item == NULL)
        return NULL;

    for(item = max_item->hh.next; item != NULL; item = item->hh.next)
        {
        if(max_item->count < item->count)
            max_item = item;
        }

    return max_item->key;
    }

static void meta_lang_clear(meta_lang_t **table)
    {
    meta_lang_t *item, *tmp;

    HASH_ITER(hh, *table, item, tmp)
        {
        HASH_DEL(*table, item);
        free(item);
        }
    }

