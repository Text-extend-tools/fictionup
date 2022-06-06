/*
Copyright 2017 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#include <yaml.h>
#include "meta_subst.h"
#include "parse_meta_lang.h"

enum parser_state_t
    {
    PS_START = 0,
    PS_LANG_KEY,
    PS_LANG_VALUE,
    PS_SUBST_KEY,
    PS_SUBST_VALUE,
    PS_SUBST_VALUES,
    PS_WAIT_END,
    PS_END
    };

int parse_meta_lang(uint8_t const *data, size_t size)
    {
    int ret = 0;
    char *lang_key = NULL;
    char *subst_key = NULL;
    enum parser_state_t state = PS_START;
    yaml_parser_t parser;

    if(size == 0)
        return ret;

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
            if(state == PS_WAIT_END)
                ret = 1;

            state = PS_END;
            break;

        case YAML_MAPPING_START_EVENT:
            if(state == PS_START)
                state = PS_LANG_KEY;
            else if(state == PS_LANG_VALUE)
                state = PS_SUBST_KEY;
            else
                state = PS_END;
            break;

        case YAML_MAPPING_END_EVENT:
            if(state == PS_SUBST_KEY || state == PS_WAIT_END)
                state = PS_WAIT_END;
            else
                state = PS_END;
            break;

        case YAML_SEQUENCE_START_EVENT:
            if(state == PS_SUBST_VALUE)
                state = PS_SUBST_VALUES;
            else
                state = PS_END;
            break;

        case YAML_SEQUENCE_END_EVENT:
            if(state == PS_SUBST_VALUES)
                state = PS_SUBST_KEY;
            else
                state = PS_END;
            break;

        case YAML_SCALAR_EVENT:
            if(state == PS_LANG_KEY)
                {
                lang_key = strdup((char *)event.data.scalar.value);
                state = PS_LANG_VALUE;
                }
            else if(state == PS_SUBST_KEY)
                {
                free(subst_key);
                subst_key = strdup((char *)event.data.scalar.value);
                state = PS_SUBST_VALUE;
                }
            else if(state == PS_SUBST_VALUE)
                {
                meta_subst_add((char *)event.data.scalar.value, subst_key, lang_key);
                state = PS_SUBST_KEY;
                }
            else if(state == PS_SUBST_VALUES)
                {
                meta_subst_add((char *)event.data.scalar.value, subst_key, lang_key);
                state = PS_SUBST_VALUES;
                }
            else
                state = PS_END;
            break;
            }

        yaml_event_delete(&event);
        }
    while(state != PS_END);

    free(subst_key);
    free(lang_key);
    yaml_parser_delete(&parser);

    return ret;
    }
