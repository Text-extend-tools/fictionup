/*
Copyright 2017 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <yaml.h>
#include "meta.h"
#include "meta_subst.h"
#include "parse_yaml.h"

enum parser_state_t
    {
    PS_START = 0,
    PS_MAP_START,
    PS_NEED_VALUE,
    PS_NEED_VALUES,
    PS_END
    };

static void store_item(meta_item_t **table, meta_item_t *new_item)
    {
    meta_item_t *item = meta_table_get(table, meta_item_key(new_item));

    if(!item)
        meta_table_add(table, new_item);
    else
        meta_item_add_item(item, new_item);
    }

int parse_yaml(meta_item_t **table, uint8_t const *data, size_t size, int def)
    {
    yaml_parser_t parser;
    enum parser_state_t state = PS_START;
    meta_item_t *cur_item = NULL;
    int ret = 1;

    if(size == 0)
        return ret;

    ret = 0;

    if(!yaml_parser_initialize(&parser))
        return ret;

    yaml_parser_set_input_string(&parser, data, size);

    do
        {
        yaml_event_t event;

        if(!yaml_parser_parse(&parser, &event))
            break;

        switch(event.type)
            {
        case YAML_NO_EVENT: break;
        case YAML_STREAM_START_EVENT: break;
        case YAML_DOCUMENT_START_EVENT: break;
        case YAML_DOCUMENT_END_EVENT: break;
        case YAML_ALIAS_EVENT: break;

        case YAML_STREAM_END_EVENT:
            ret = 1;
            state = PS_END;
            break;

        case YAML_MAPPING_START_EVENT:
            state = PS_MAP_START;
            break;

        case YAML_MAPPING_END_EVENT:
            state = PS_START;
            break;

        case YAML_SCALAR_EVENT:
            if(state == PS_MAP_START)
                {
                char const *subst;

                if(def)
                    subst = meta_subst_def(((char *)event.data.scalar.value));
                else
                    subst = meta_subst(((char *)event.data.scalar.value));

                cur_item = meta_item_new(subst);
                state = PS_NEED_VALUE;
                }
            else if(state == PS_NEED_VALUE)
                {
                if(cur_item != NULL)
                    {
                    meta_item_add_value(cur_item, event.data.scalar.value);
                    store_item(table, cur_item);
                    cur_item = NULL;
                    }

                state = PS_MAP_START;
                }
            else if(state == PS_NEED_VALUES)
                {
                if(cur_item != NULL)
                    meta_item_add_value(cur_item, event.data.scalar.value);
                }
            break;

        case YAML_SEQUENCE_START_EVENT:
            if(state == PS_NEED_VALUE)
                state = PS_NEED_VALUES;
            break;

        case YAML_SEQUENCE_END_EVENT:
            if(state == PS_NEED_VALUES)
                {
                if(cur_item != NULL)
                    {
                    if(meta_item_count_values(cur_item) != 0)
                        store_item(table, cur_item);
                    else
                        meta_item_del(cur_item);

                    cur_item = NULL;
                    }

                state = PS_MAP_START;
                }
            break;
            }

        yaml_event_delete(&event);
        }
    while(state != PS_END);

    yaml_parser_delete(&parser);

    if(cur_item != NULL)
        meta_item_del(cur_item);

    return ret;
    }

void parse_guess_lang(meta_item_t **table)
    {
    char const *lang_key = "lang";

    if(meta_table_get(table, lang_key) == NULL)
        {
        char const *lang = meta_subst_get_lang();

        if(lang != NULL && *lang != '\0')
            {
            meta_item_t *item = meta_item_new(lang_key);
            meta_item_add_value(item, (unsigned char *)lang);
            meta_table_add(table, item);
            }
        }
    }
