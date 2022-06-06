/*
Copyright 2017-2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE /* memmem */
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include "document.h"
#include "b64/cencode.h"
#include "parse_yaml.h"
#include "uthash.h"
#include "utils.h"
#include "version.h"
#include "fb2.h"

#define TEXT_BUF_UNIT 1024
#define BINARY_BUF_UNIT 8192
#define MAX_NESTING_ANN 16

static char const TagA[] = "<a";
static char const TagAuthor[] = "<author";
static char const TagBinary[] = "<binary";
static char const TagBody[] = "<body";
static char const TagBookTitle[] = "<book-title";
static char const TagCoverPage[] = "<coverpage";
static char const TagCite[] = "<cite";
static char const TagCode[] = "<code";
static char const TagDate[] = "<date";
static char const TagDescription[] = "<description";
static char const TagDocumentInfo[] = "<document-info";
static char const TagEmphasis[] = "<emphasis";
static char const TagFictionBook[] = "<FictionBook";
static char const TagFirstName[] = "<first-name";
static char const TagGenre[] = "<genre";
static char const TagId[] = "<id";
static char const TagImage[] = "<image";
static char const TagLang[] = "<lang";
static char const TagLastName[] = "<last-name";
static char const TagMiddleName[] = "<middle-name";
#if 0
static char const TagNickname[] = "<nickname";
#endif
static char const TagP[] = "<p";
static char const TagSection[] = "<section";
static char const TagStrikethrough[] = "<strikethrough";
static char const TagStrong[] = "<strong";
static char const TagSup[] = "<sup";
static char const TagTable[] = "<table";
static char const TagTd[] = "<td";
static char const TagTh[] = "<th";
static char const TagTitleInfo[] = "<title-info";
static char const TagTitle[] = "<title";
static char const TagTr[] = "<tr";
static char const TagVersion[] = "<version";
static char const TagEmptyLine[] = "<empty-line/>\n";
static char const TagProgram[] = "<program-used";
static char const TagAnnotation[] = "<annotation";
static char const TagSequence[] = "<sequence";

static char const AttrAlt[] = " alt=";
static char const AttrContentType[] = " content-type=";
static char const AttrHref[] = " xlink:href=";
static char const AttrId[] = " id=";
static char const AttrTitle[] = " title=";
static char const AttrValue[] = " value=";
static char const AttrXmlnsXlink[] = " xmlns:xlink=";
static char const AttrXmlns[] = " xmlns=";
static char const AttrName[] = " name=";
static char const AttrNumber[] = " number=";

static char const NotePrefix[] = "note_";

static char const DefListIndent[] = "\xC2\xA0\xC2\xA0\xC2\xA0";
static char const DefListSpacing[] = "\xC2\xA0";
static char const DefListMarker[] = "\xE2\x80\xA2";
static char const DefListDelimiter[] = ".";

typedef struct image_list
    {
    uint8_t *image_id;
    size_t size;
    UT_hash_handle hh;
    } image_list_t;

typedef struct fb2_state
    {
    fb2_config_t cfg;
    hoedown_buffer const *data_dir;
    size_t header_size;
    size_t last_header;
    int section_level;
    int body_closed;
    int list_idx[FB2_LIST_DEPTH];
    int in_citation;
    hoedown_buffer *yaml;
    hoedown_buffer *binary;
    hoedown_buffer *book_title;
    image_list_t *images;
    } fb2_state_t;

static void fill_array(char const *array[], unsigned n, char const *def)
    {
    unsigned i;

    if(array[0] == NULL)
        array[0] = def;

    for(i = 1; i < n; ++i)
        {
        if(array[i] == NULL)
            array[i] = array[i - 1];
        }
    }

static void prepare_config(fb2_config_t *cfg)
    {
    fill_array(cfg->list_indent, FB2_LIST_DEPTH, DefListIndent);
    fill_array(cfg->list_spacing, FB2_LIST_DEPTH, DefListSpacing);
    fill_array(cfg->list_marker, FB2_LIST_DEPTH, DefListMarker);
    fill_array(cfg->list_delimiter, FB2_LIST_DEPTH, DefListDelimiter);
    }

static image_list_t *image_list_new(uint8_t const *data, size_t size)
    {
    image_list_t *item = hoedown_calloc(1, sizeof(*item) + size);

    item->image_id = (uint8_t *)(item + 1);
    item->size = size;

    memcpy(item->image_id, data, size);

    return item;
    }

void image_list_delete(image_list_t *item)
    {
    free(item);
    }

static fb2_state_t *fb2_state_new(fb2_config_t const *config, hoedown_buffer const *data_dir)
    {
    fb2_state_t *state = hoedown_calloc(1, sizeof(*state));

    if(config != NULL)
        state->cfg = *config;

    prepare_config(&state->cfg);

    state->data_dir = data_dir;

    state->header_size = 0;
    state->last_header = 0;
    state->section_level = 0;
    state->body_closed = 0;
    state->in_citation = 0;

    memset(state->list_idx, 0, sizeof(state->list_idx));
    
    state->yaml = hoedown_buffer_new(TEXT_BUF_UNIT);
    state->binary = hoedown_buffer_new(BINARY_BUF_UNIT);
    state->book_title = hoedown_buffer_new(TEXT_BUF_UNIT);

    state->images = NULL;

    return state;
    }

static void fb2_state_delete(fb2_state_t *state)
    {
    image_list_t *item, *tmp;

    if(state == NULL)
        return;

    HASH_ITER(hh, state->images, item, tmp)
        {
        HASH_DEL(state->images, item);
        image_list_delete(item);
        }

    hoedown_buffer_free(state->book_title);
    hoedown_buffer_free(state->binary);
    hoedown_buffer_free(state->yaml);
    free(state);
    }

static void hb_open_tag(hoedown_buffer *ob, char const *tag)
    {
    hoedown_buffer_puts(ob, tag);
    hoedown_buffer_putc(ob, '>');
    }

static void hb_open_tag_nl(hoedown_buffer *ob, char const *tag)
    {
    hb_open_tag(ob, tag);
    hoedown_buffer_putc(ob, '\n');
    }

static void hb_open_tag_attr(hoedown_buffer *ob, char const *tag, char const *attr)
    {
    hoedown_buffer_puts(ob, tag);

    if(attr != NULL)
        hoedown_buffer_puts(ob, attr);

    hoedown_buffer_putc(ob, '>');
    }

static void hb_close_tag(hoedown_buffer *ob, char const *tag)
    {
    hoedown_buffer_puts(ob, "</");
    hoedown_buffer_puts(ob, tag + 1);
    hoedown_buffer_puts(ob, ">");
    }

static void hb_close_tag_nl(hoedown_buffer *ob, char const *tag)
    {
    hb_close_tag(ob, tag);
    hoedown_buffer_putc(ob, '\n');
    }

static void hb_put_text1(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag)
    {
    hb_open_tag(ob, tag);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag(ob, tag);
    }

static void hb_put_text1_nl(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag)
    {
    hb_open_tag(ob, tag);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag_nl(ob, tag);
    }

static void hb_put_text2(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag1, char const *tag2)
    {
    hb_open_tag(ob, tag1);
    hb_open_tag(ob, tag2);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag(ob, tag2);
    hb_close_tag(ob, tag1);
    }

static void hb_put_text2_nl(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag1, char const *tag2)
    {
    hb_open_tag(ob, tag1);
    hb_open_tag(ob, tag2);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag(ob, tag2);
    hb_close_tag_nl(ob, tag1);
    }

static void hb_put_text1_attr(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag, char const *attr)
    {
    hb_open_tag_attr(ob, tag, attr);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag(ob, tag);
    }

static void hb_put_text1_attr_nl(hoedown_buffer *ob, hoedown_buffer const *content, char const *tag, char const *attr)
    {
    hb_open_tag_attr(ob, tag, attr);
    hoedown_buffer_put(ob, content->data, content->size);
    hb_close_tag_nl(ob, tag);
    }

static void hb_escape_data(hoedown_buffer *ob, uint8_t const *data, size_t size, int keep_fmt)
    {
    size_t pos1 = 0;
    size_t pos2;
    char const *replace_fmt = (keep_fmt ? NULL : " ");

    for(pos2 = 0; pos2 < size; ++pos2)
        {
        char const *replace = NULL;

        switch(data[pos2])
            {
        case '&': replace = "&amp;"; break;
        case '<': replace = "&lt;"; break;
        case '>': replace = "&gt;"; break;
        case '\t': replace = replace_fmt; break;
        case '\n': replace = replace_fmt; break;
        case '\r': replace = replace_fmt; break;
        default: if(data[pos2] < ' ') replace = " ";
            }

        if(replace)
            {
            if(pos2 > pos1)
                hoedown_buffer_put(ob, data + pos1, pos2 - pos1);

            if(ob->size > 0 && ob->data[ob->size - 1] == ' ')
                {
                while(*replace == ' ')
                    ++replace;
                }

            hoedown_buffer_puts(ob, replace);

            pos1 = pos2 + 1;
            }
        }

    if(pos2 > pos1)
        hoedown_buffer_put(ob, data + pos1, pos2 - pos1);
    }

static void hb_escape_text(hoedown_buffer *ob, const hoedown_buffer *text, int keep_fmt)
    {
    hb_escape_data(ob, text->data, text->size, keep_fmt);
    }

static void hb_escape_str(hoedown_buffer *ob, unsigned char const *str, int keep_fmt)
    {
    hb_escape_data(ob, str, strlen((char *)str), keep_fmt);
    }

static void hb_put_str1(hoedown_buffer *ob, char const *str, char const *tag)
    {
    hb_open_tag(ob, tag);
    hb_escape_str(ob, (unsigned char *)str, 0);
    hb_close_tag(ob, tag);
    }

static void hb_put_str1_nl(hoedown_buffer *ob, char const *str, char const *tag)
    {
    hb_open_tag(ob, tag);
    hb_escape_str(ob, (unsigned char *)str, 0);
    hb_close_tag_nl(ob, tag);
    }

static void fb2_blockcode(hoedown_buffer *ob, const hoedown_buffer *text,
    const hoedown_buffer *lang, const hoedown_renderer_data *data)
    {
    (void)lang; /* Unused */
    (void)data; /* Unused */

    if(text == NULL)
        return;

    hb_open_tag(ob, TagP);
    hb_open_tag(ob, TagCode);
    hb_escape_text(ob, text, 1);
    hb_close_tag(ob, TagCode);
    hb_close_tag_nl(ob, TagP);
    }

static void fb2_paragraph(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    /* Paragraph with single '\' considered to be empty line */
    if(content->size == 1 && content->data[0] == '\\')
        hoedown_buffer_puts(ob, TagEmptyLine);

    else
        hb_put_text1_nl(ob, content, TagP);
    }

static void fb2_blockquote(hoedown_buffer *ob, const hoedown_buffer *content, int level, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    if(level == 1)
        hb_put_text1_nl(ob, content, TagCite);
    else
        hoedown_buffer_put(ob, content->data, content->size);
    }

static int list_i(int level)
    {
    --level;

    if(level < 0)
        level = 0;
    else if(level > FB2_LIST_DEPTH - 1)
        level = FB2_LIST_DEPTH - 1;

    return level;
    }

static int get_list_idx(fb2_state_t *state, int level)
    {
    return ++state->list_idx[list_i(level)];
    }

static void reset_list_idx(fb2_state_t *state, int level)
    {
    state->list_idx[list_i(level)] = 0;
    }

static void fb2_list(hoedown_buffer *ob, const hoedown_buffer *content,
    hoedown_list_flags flags, int level, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;

    (void)flags; /* Unused */

    if(content == NULL)
        return;

    hoedown_buffer_put(ob, content->data, content->size);
    reset_list_idx(state, level);
    }

static size_t find_tag(uint8_t const *data, size_t sz, char const *tag)
    {
    size_t tag_len = strlen(tag);
    uint8_t const *found = data - 1;
    size_t i = sz;

    do
        {
        ++found;

        found = memmem(found, data + sz - found, tag, tag_len);

        if(found == NULL)
            return sz;

        i = found - data;

        found += tag_len;

        if(found >= data + sz)
            return sz;
        }
    while(*found != '>' && *found != ' ');

    return i;
    }

static void fb2_listitem(hoedown_buffer *ob, const hoedown_buffer *content,
    hoedown_list_flags flags, int level, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;
    int li = list_i(level);
    int i;
    size_t insert_pos, insert_flag, skip_eol;

    if(content == NULL)
        return;

    hb_open_tag(ob, TagP);

    for(i = 0; i < level; ++i)
        hoedown_buffer_puts(ob, state->cfg.list_indent[li]);

    if((flags & HOEDOWN_LIST_ORDERED) != 0)
        hoedown_buffer_printf(ob, "%d%s%s",
            get_list_idx(state, level),
            state->cfg.list_delimiter[li],
            state->cfg.list_spacing[li]
            );
    else
        hoedown_buffer_printf(ob, "%s%s",
            state->cfg.list_marker[li],
            state->cfg.list_spacing[li]
            );

    insert_pos = find_tag(content->data, content->size, TagP);

    if(insert_pos < content->size)
        insert_flag = 1;
    else
        {
        insert_pos = 0;
        insert_flag = 0;
        }

    if(insert_flag)
        {
        if(insert_pos > 0)
            {
            skip_eol = (content->data[insert_pos - 1] == '\n' ? 1 : 0);
            hoedown_buffer_put(ob, content->data, insert_pos - skip_eol);
            }

        hb_close_tag_nl(ob, TagP);
        }

    skip_eol = (!insert_flag && content->data[content->size - 1] == '\n' ? 1 : 0);

    hoedown_buffer_put(ob, content->data + insert_pos, content->size - insert_pos - skip_eol);

    if(!insert_flag)
        hb_close_tag_nl(ob, TagP);
    }

static void hb_concat(hoedown_buffer *ob, uint8_t const *data, size_t sz)
    {
#if 0
    if(ob->size > 0 && !isspace(ob->data[ob->size - 1]) && !isspace(data[0]))
        hoedown_buffer_putc(ob, ' ');
#endif

    hb_escape_data(ob, data, sz, 0);
    }

static void hb_skip_tags(hoedown_buffer *ob, const hoedown_buffer *text)
    {
    int depth = 0;
    size_t pos1 = 0;
    size_t pos2;

    for(pos2 = 0; pos2 < text->size; ++pos2)
        {
        if(text->data[pos2] == '<')
            {
            if(pos2 > pos1 && depth <= 0)
                hb_concat(ob, text->data + pos1, pos2 - pos1);

            pos1 = pos2;

            ++depth;
            }
        else if(text->data[pos2] == '>')
            {
            /* Skip from pos1 to pos2 */
            --depth;
            pos1 = pos2 + 1;
            }
        }

    if(pos2 > pos1 && depth <= 0)
        hb_concat(ob, text->data + pos1, pos2 - pos1);
    }

static void fb2_hrule(hoedown_buffer *ob, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    hoedown_buffer_puts(ob, "<subtitle>* * *</subtitle>\n");
    }

static void hb_close_sections(hoedown_buffer *ob, int level, fb2_state_t *state)
    {
    int once = 1;

    while(state->section_level >= level)
        {
#if 0
        if(ob->size == state->last_header)
            fb2_hrule(ob, data); /* Section was empty, put something here */
#endif

        if(once)
            {
            hoedown_buffer_puts(ob, TagEmptyLine);
            once = 0;
            }

        hb_close_tag_nl(ob, TagSection);
        --state->section_level;
        }
    }

static void fb2_header(
    hoedown_buffer *ob, const hoedown_buffer *content, int level, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;

    if(content == NULL)
        return;

    /* No need to close empty first section */
    if(ob->size != state->header_size)
        hb_close_sections(ob, level, state);

    while(state->section_level < level)
        {
        hb_open_tag(ob, TagSection);
        ++state->section_level;
        }

    hb_put_text2_nl(ob, content, TagTitle, TagP);

    state->last_header = ob->size;

    if(state->book_title->size == 0)
        hb_skip_tags(state->book_title, content);
    }

static void fb2_annotation_header(
    hoedown_buffer *ob, const hoedown_buffer *content, int level, const hoedown_renderer_data *data)
    {
    (void)level; /* Unused */
    fb2_paragraph(ob, content, data);
    }

static void fb2_yaml_meta(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;

    if(content == NULL)
        return;

    (void)ob; /* Unused */

    hoedown_buffer_put(state->yaml, content->data, content->size);
    }

static void fb2_table(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hb_put_text1_nl(ob, content, TagTable);
    }

static void fb2_table_header(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hoedown_buffer_put(ob, content->data, content->size);
    }

static void fb2_table_body(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hoedown_buffer_put(ob, content->data, content->size);
    }

static void fb2_table_row(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hb_put_text1_nl(ob, content, TagTr);
    }

static void fb2_table_cell(hoedown_buffer *ob, const hoedown_buffer *content, hoedown_table_flags flags, const hoedown_renderer_data *data)
    {
    static char const attr_center[] = " align=\"center\"";
    static char const attr_right[] = " align=\"right\"";
    char const *tag = TagTd;
    char const *attr = NULL;

    (void)data; /* Unused */

    if(content == NULL)
        return;

    if((flags & HOEDOWN_TABLE_HEADER) != 0)
        tag = TagTh;

    switch(flags & HOEDOWN_TABLE_ALIGNMASK)
        {
    case HOEDOWN_TABLE_ALIGN_LEFT:
        break;

    case HOEDOWN_TABLE_ALIGN_RIGHT:
        attr = attr_right;
        break;

    case HOEDOWN_TABLE_ALIGN_CENTER:
        attr = attr_center;
        break;

    default:
        break;
        }

    hb_put_text1_attr(ob, content, tag, attr);
    }

static void fb2_close_body(hoedown_buffer *ob, fb2_state_t *state)
    {
    if(state->body_closed)
        return;

    hb_close_sections(ob, 1, state);
    hb_close_tag_nl(ob, TagBody);

    state->body_closed = 1;
    }

static void fb2_footnotes(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    if(content == NULL)
        return;

    fb2_close_body(ob, data->opaque);
    hb_put_text1_attr_nl(ob, content, TagBody, " name=\"notes\"");
    }

static void fb2_footnote_def(hoedown_buffer *ob, const hoedown_buffer *content, unsigned int num, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hoedown_buffer_printf(ob, "%s%s\"%s%u\">", TagSection, AttrId, NotePrefix, num);

    hb_open_tag(ob, TagTitle);
    hb_open_tag(ob, TagP);
    hoedown_buffer_printf(ob, "%u", num);
    hb_close_tag(ob, TagP);
    hb_close_tag_nl(ob, TagTitle);

    hoedown_buffer_put(ob, content->data, content->size);

    hb_close_tag_nl(ob, TagSection);
    }

static int fb2_footnote_ref(hoedown_buffer *ob, unsigned int num, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    hoedown_buffer_printf(ob, "%s%s\"#%s%u\" type=\"note\">%u</%s>",
        TagA, AttrHref, NotePrefix, num, num, TagA + 1);

    return 1;
    }

static int fb2_raw_html(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_skip_tags(ob, content);

    return 1;
    }

static void fb2_blockhtml(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    if(content == NULL)
        return;

    hb_open_tag(ob, TagP);
    fb2_raw_html(ob, content, data);
    hb_close_tag_nl(ob, TagP);
    }

static void hb_format_attr_raw(hoedown_buffer *ob, char const *attr, void const *data, size_t size)
    {
    hoedown_buffer_puts(ob, attr);
    hoedown_buffer_putc(ob, '"');
    hb_escape_data(ob, data, size, 0);
    hoedown_buffer_putc(ob, '"');
    }

static void hb_format_attr(hoedown_buffer *ob, char const *attr, hoedown_buffer const *value)
    {
    hb_format_attr_raw(ob, attr, value->data, value->size);
    }

static void hb_format_attrs(hoedown_buffer *ob, char const *attr, char const *value)
    {
    hb_format_attr_raw(ob, attr, value, strlen(value));
    }

static int fb2_autolink(hoedown_buffer *ob, const hoedown_buffer *link,
    hoedown_autolink_type type, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(link == NULL)
        return 0;

    switch(type)
        {
    case HOEDOWN_AUTOLINK_NORMAL:
        hoedown_buffer_puts(ob, TagA);
        hb_format_attr(ob, AttrHref, link);
        hoedown_buffer_puts(ob, ">");
        hoedown_buffer_put(ob, link->data, link->size);
        hb_close_tag(ob, TagA);
        break;

    default:
        hoedown_buffer_put(ob, link->data, link->size);
        }

    return 1;
    }

int fb2_codespan(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_open_tag(ob, TagCode);
    hb_escape_text(ob, content, 1);
    hb_close_tag(ob, TagCode);

    return 1;
    }

static int fb2_double_emphasis(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text1(ob, content, TagStrong);

    return 1;
    }

static int fb2_triple_emphasis(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text2(ob, content, TagStrong, TagEmphasis);

    return 1;
    }

static int fb2_emphasis(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text1(ob, content, TagEmphasis);

    return 1;
    }

#if 0
static int fb2_quote(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text1(ob, content, TagCite);

    return 1;
    }
#endif

static int fb2_link(hoedown_buffer *ob, const hoedown_buffer *content,
    const hoedown_buffer *link, const hoedown_buffer *title, const hoedown_renderer_data *data)
    {
    (void)title; /* Unused */
    (void)data; /* Unused */

    if(content == NULL || content->size == 0)
        content = link;

    if(content == NULL || content->size == 0)
        return 0;

    if(link != NULL && link->size > 0)
        {
        hoedown_buffer_puts(ob, TagA);
        hb_format_attr(ob, AttrHref, link);
        hoedown_buffer_puts(ob, ">");
        }

    hoedown_buffer_put(ob, content->data, content->size);

    if(link != NULL && link->size > 0)
        hb_close_tag(ob, TagA);

    return 1;
    }
    
static int fb2_strikethrough(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text1(ob, content, TagStrikethrough);

    return 1;
    }

static int fb2_superscript(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return 0;

    hb_put_text1(ob, content, TagSup);

    return 1;
    }

static void fb2_normal_text(hoedown_buffer *ob, const hoedown_buffer *content, const hoedown_renderer_data *data)
    {
    (void)data; /* Unused */

    if(content == NULL)
        return;

    hb_escape_text(ob, content, 0);
    }

static hoedown_buffer *extract_file_path(hoedown_buffer const *link)
    {
    hoedown_buffer *file_path = hoedown_buffer_new(link->size + 1);
    uint8_t const *found;

    hoedown_buffer_set(file_path, link->data, link->size);

    found = (uint8_t const *)strstr(hoedown_buffer_cstr(file_path), "://");

    if(found != NULL) /* URL */
        {
        found = link->data + (found - file_path->data) + 3;
        found = memchr(found, '/', link->size - (found - link->data));

        if(found != NULL)
            hoedown_buffer_set(file_path, found, link->size - (found - link->data));
        }

    return file_path;
    }

static hoedown_buffer *extract_file_name(hoedown_buffer const *link)
    {
    hoedown_buffer *file_name = hoedown_buffer_new(link->size);
    uint8_t const *found;

    found = mem_last_path_part(link->data, link->size);

    hoedown_buffer_set(file_name, found, link->size - (found - link->data));

    return file_name;
    }

static hoedown_buffer *read_image_file(hoedown_buffer const *link, hoedown_buffer const *data_dir)
    {
    hoedown_buffer *image_path = NULL;
    hoedown_buffer *image_name = NULL;
    hoedown_buffer *data_path = NULL;
    FILE *in = NULL;
    hoedown_buffer *image = NULL;
    hoedown_buffer *base64 = NULL;
    size_t base64_size;
    base64_encodestate b64;

    /* Extract file name */
    image_path = extract_file_path(link);
    image_name = extract_file_name(image_path);

    do
        {
        /* Search image file */
        if(data_dir != NULL)
            {
            /* image_path in data_dir */
            data_path = hoedown_buffer_new(link->size);
            concat_path_data(data_path, data_dir, image_path->data, image_path->size);
            in = fopen(hoedown_buffer_cstr(data_path), "rb");

            if(in == NULL)
                {
                /* image_name in data_dir */
                concat_path_data(data_path, data_dir, image_name->data, image_name->size);
                in = fopen(hoedown_buffer_cstr(data_path), "rb");
                }
            }

        if(in == NULL)
            /* image_path as is */
            in = fopen(hoedown_buffer_cstr(image_path), "rb");

        if(in == NULL && is_path_separator(image_path->data[0]))
            {
            /* relative image_path */
            hoedown_buffer_slurp(image_path, 1);
            in = fopen(hoedown_buffer_cstr(image_path), "rb");
            }

        if(in == NULL)
            /* image_name */
            in = fopen(hoedown_buffer_cstr(image_name), "rb");

        if(in == NULL)
            {
            fprintf(stderr, "WARNING: Can't open image file: %s\n", hoedown_buffer_cstr(image_path));
            perror("WARNING");
            break;
            }

        /* Read whole file */
        image = hoedown_buffer_new(BINARY_BUF_UNIT);
        hoedown_buffer_putf(image, in);

        if(image->size == 0)
            {
            fprintf(stderr, "WARNING: Can't read image file: %s\n", hoedown_buffer_cstr(image_path));
            break;
            }

        /* Encode to base64 */
        base64_size = ((image->size * 4) / 3) + (image->size / (72 * 3 / 4)) + 6;

        base64 = hoedown_buffer_new(BINARY_BUF_UNIT);
        hoedown_buffer_grow(base64, base64_size);

        base64_init_encodestate(&b64);

        base64_size = base64_encode_block(image->data, image->size, (char *)base64->data, &b64);
        base64_size += base64_encode_blockend((char *)base64->data + base64_size, &b64);

        assert(base64->asize >= base64_size);

        base64->size = base64_size;
        }
    while(0);

    hoedown_buffer_free(image);
    if(in != NULL) fclose(in);
    hoedown_buffer_free(data_path);
    hoedown_buffer_free(image_name);
    hoedown_buffer_free(image_path);

    return base64;
    }

static char const *image_content_type(hoedown_buffer const *base64)
    {
    static char const sig_jpg[] = "/9j/";
    static char const type_jpg[] = "image/jpeg";
    static char const sig_png[] = "iVBORw0K";
    static char const type_png[] = "image/png";

    if(hoedown_buffer_prefix(base64, sig_jpg) == 0)
        return type_jpg;

    if(hoedown_buffer_prefix(base64, sig_png) == 0)
        return type_png;

    return NULL;
    }

static char const *store_image(
    hoedown_buffer *binary,
    hoedown_buffer const *link,
    hoedown_buffer const *data_dir,
    hoedown_buffer const *image_id
    )
    {
    hoedown_buffer *base64;
    char const *content_type;

    base64 = read_image_file(link, data_dir);
    if(base64 == NULL)
        return NULL;

    content_type = image_content_type(base64);

    if(content_type == NULL)
        fprintf(stderr, "WARNING: Unknown image type: %.*s\n", (int)link->size, link->data);
    else
        {
        /* <binary id="cover.jpg" content-type="image/jpeg"> */
        hoedown_buffer_puts(binary, TagBinary);
        hb_format_attr(binary, AttrId, image_id);
        hb_format_attrs(binary, AttrContentType, content_type);
        hoedown_buffer_puts(binary, ">\n");
        hoedown_buffer_put(binary, base64->data, base64->size);
        hb_close_tag_nl(binary, TagBinary);
        }

    hoedown_buffer_free(base64);

    return content_type;
    }

static hoedown_buffer *make_image_id(hoedown_buffer const *link)
    {
    hoedown_buffer *id = extract_file_name(link);
    size_t i;

    for(i = 0; i < id->size; ++i)
        {
        if(id->data[i] == ':' || id->data[i] == '/' || id->data[i] == '\\')
            id->data[i] = '_';
        }

    return id;
    }

static int fb2_image(hoedown_buffer *ob, const hoedown_buffer *link,
    const hoedown_buffer *title, const hoedown_buffer *alt, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;
    hoedown_buffer *image_id;
    image_list_t *item;

    if(link == NULL || link->size == 0)
        return 1;

    image_id = make_image_id(link);

    HASH_FIND(hh, state->images, image_id->data, image_id->size, item);

    if(item == NULL)
        {
        if(store_image(state->binary, link, state->data_dir, image_id))
            {
            item = image_list_new(image_id->data, image_id->size);
            HASH_ADD_KEYPTR(hh, state->images, item->image_id, item->size, item);
            }
        }

    if(item != NULL)
        {
        /* <image l:href="#picture.jpg"/> */
        hoedown_buffer_puts(ob, TagImage);
        hoedown_buffer_puts(ob, AttrHref);
        hoedown_buffer_puts(ob, "\"#");
        hoedown_buffer_put(ob, image_id->data, image_id->size);
        hoedown_buffer_putc(ob, '"');

        if(alt != NULL && alt->size > 0)
            hb_format_attr(ob, AttrAlt, alt);

        if(title != NULL && title->size > 0)
            hb_format_attr(ob, AttrTitle, title);

        hoedown_buffer_puts(ob, " />");
        }

    hoedown_buffer_free(image_id);

    return 1;
    }

static void fb2_doc_header(hoedown_buffer *ob, int inline_render, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;

    (void)inline_render; /* Unused */

    hb_open_tag(ob, TagSection);

    ++state->section_level;

    state->header_size = ob->size;
    }

static void fb2_doc_footer(hoedown_buffer *ob, int inline_render, const hoedown_renderer_data *data)
    {
    fb2_state_t *state = data->opaque;

    (void)inline_render; /* Unused */

    fb2_close_body(ob, state);
    }

hoedown_renderer *fb2_renderer_new(fb2_config_t const *config, hoedown_buffer const *data_dir)
    {
    hoedown_renderer *r = hoedown_calloc(1, sizeof(*r));

    r->blockcode = fb2_blockcode;
    r->blockquote = fb2_blockquote;
    r->list = fb2_list;
    r->listitem = fb2_listitem;
    r->header = fb2_header;
    r->hrule = fb2_hrule;
    r->yaml_meta = fb2_yaml_meta;
    r->paragraph = fb2_paragraph;
    r->table = fb2_table;
    r->table_header = fb2_table_header;
    r->table_body = fb2_table_body;
    r->table_row = fb2_table_row;
    r->table_cell = fb2_table_cell;
    r->footnotes = fb2_footnotes;
    r->footnote_def = fb2_footnote_def;
    r->footnote_ref = fb2_footnote_ref;
    r->blockhtml = fb2_blockhtml;
    r->autolink = fb2_autolink;
    r->codespan = fb2_codespan;
    r->double_emphasis = fb2_double_emphasis;
    r->triple_emphasis = fb2_triple_emphasis;
    r->emphasis = fb2_emphasis;
#if 0
    r->quote = fb2_quote;
#endif
    r->link = fb2_link;
    r->strikethrough = fb2_strikethrough;
    r->superscript = fb2_superscript;
    r->raw_html = fb2_raw_html;
    r->normal_text = fb2_normal_text;
    r->image = fb2_image;
    r->doc_footer = fb2_doc_footer;
    r->doc_header = fb2_doc_header;

    r->opaque = fb2_state_new(config, data_dir);

    return r;
    }

static hoedown_renderer *fb2_annotation_new(fb2_config_t const *config)
    {
    hoedown_renderer *r = hoedown_calloc(1, sizeof(*r));

    r->blockcode = fb2_blockcode;
    r->blockquote = fb2_blockquote;
    r->list = fb2_list;
    r->listitem = fb2_listitem;
    r->header = fb2_annotation_header;
    r->hrule = fb2_hrule;
    r->yaml_meta = NULL; /* No YAML allowed in annotation */
    r->paragraph = fb2_paragraph;
    r->table = fb2_table;
    r->table_header = fb2_table_header;
    r->table_body = fb2_table_body;
    r->table_row = fb2_table_row;
    r->table_cell = fb2_table_cell;
    r->footnotes = fb2_footnotes;
    r->footnote_def = fb2_footnote_def;
    r->footnote_ref = fb2_footnote_ref;
    r->blockhtml = fb2_blockhtml;
    r->autolink = fb2_autolink;
    r->codespan = fb2_codespan;
    r->double_emphasis = fb2_double_emphasis;
    r->triple_emphasis = fb2_triple_emphasis;
    r->emphasis = fb2_emphasis;
    r->link = fb2_link;
    r->strikethrough = fb2_strikethrough;
    r->superscript = fb2_superscript;
    r->raw_html = fb2_raw_html;
    r->normal_text = fb2_normal_text;
    r->image = NULL; /* No images allowed in annotation */
    r->doc_footer = NULL; /* No special action at the end */
    r->doc_header = NULL; /* No special action at the begin */

    r->opaque = fb2_state_new(config, NULL);

    return r;
    }

void fb2_renderer_delete(hoedown_renderer *r)
    {
    if(r == NULL)
        return;

    fb2_state_delete(r->opaque);
    free(r);
    }

int fb2_get_extensions(void)
    {
    return
        HOEDOWN_EXT_YAML_META
        | HOEDOWN_EXT_TABLES
        | HOEDOWN_EXT_FOOTNOTES
        | HOEDOWN_EXT_SUPERSCRIPT
        /*| HOEDOWN_EXT_QUOTE*/
        | HOEDOWN_EXT_FENCED_CODE
        | HOEDOWN_EXT_STRIKETHROUGH;
    }

static int fb2_annotation_extensions(void)
    {
    return
        HOEDOWN_EXT_TABLES
        | HOEDOWN_EXT_FOOTNOTES
        | HOEDOWN_EXT_SUPERSCRIPT
        | HOEDOWN_EXT_FENCED_CODE
        | HOEDOWN_EXT_STRIKETHROUGH;
    }

hoedown_buffer *fb2_get_yaml(hoedown_renderer *r)
    {
    fb2_state_t *state = r->opaque;

    return state->yaml;
    }

static meta_item_t const *get_meta_or_def(meta_item_t *meta, meta_item_t *def, char const *key)
    {
    meta_item_t const *item;

    item = meta_table_get(&meta, key);

    if(!item)
        item = meta_table_get(&def, key);

    if(!item)
        item = meta_item_get_unknown();

    return item;
    }

static void hb_put_value(hoedown_buffer *ob, char const *tag, meta_item_t const *item, unsigned i)
    {
    hb_open_tag(ob, tag);
    hb_escape_str(ob, meta_item_value(item, i), 0);
    hb_close_tag(ob, tag);
    }

static void hb_put_value_nl(hoedown_buffer *ob, char const *tag, meta_item_t const *item, unsigned i)
    {
    hb_open_tag(ob, tag);
    hb_escape_str(ob, meta_item_value(item, i), 0);
    hb_close_tag_nl(ob, tag);
    }

static void hb_put_values(hoedown_buffer *ob, char const *tag, meta_item_t const *item)
    {
    unsigned n, i;

    n = meta_item_count_values(item);

    for(i = 0; i < n; ++i)
        hb_put_value(ob, tag, item, i);

    hoedown_buffer_putc(ob, '\n');
    }

static void hb_join_values(hoedown_buffer *ob, char const *tag, meta_item_t const *item, unsigned from, unsigned to)
    {
    unsigned i;

    hb_open_tag(ob, tag);
    hb_escape_str(ob, meta_item_value(item, from), 0);

    for(i = from + 1; i <= to; ++i)
        {
        hoedown_buffer_putc(ob, ' ');
        hb_escape_str(ob, meta_item_value(item, i), 0);
        }

    hb_close_tag(ob, tag);
    }

static void hb_put_author(hoedown_buffer *ob, meta_item_t const *item)
    {
    unsigned n;
    meta_item_t *parts = NULL;

    n = meta_item_count_values(item);

    if(n == 1)
        {
        parts = meta_item_split_value(item, 0);

        if(parts != NULL)
            {
            item = parts;
            n = meta_item_count_values(item);
            }
        }

    if(n == 0)
        return;

    hb_open_tag(ob, TagAuthor);

    if(n == 1)
        {
        hb_put_str1(ob, "", TagFirstName);
        hb_put_value(ob, TagLastName, item, 0);
        }
    else if(n == 2)
        {
        hb_put_value(ob, TagFirstName, item, 0);
        hb_put_value(ob, TagLastName, item, 1);
        }
    else if(n == 3)
        {
        hb_put_value(ob, TagFirstName, item, 0);
        hb_put_value(ob, TagMiddleName, item, 1);
        hb_put_value(ob, TagLastName, item, 2);
        }
    else if(n > 3)
        {
#if 0
        hb_join_values(ob, TagNickname, item); /* Nickname not supported by most viewers */
#endif
        hb_put_value(ob, TagFirstName, item, 0);
        hb_join_values(ob, TagMiddleName, item, 1, n - 2);
        hb_put_value(ob, TagLastName, item, n - 1);
        }

    hb_close_tag_nl(ob, TagAuthor);

    meta_item_del(parts);
    }

static void hb_put_current_date(hoedown_buffer *ob)
    {
    time_t now = time(NULL);
    struct tm *loc = localtime(&now);

    hoedown_buffer_printf(ob, "%s%s\"%04d-%02d-%02d\">%04d-%02d-%02d</%s>\n",
        TagDate, AttrValue,
        loc->tm_year + 1900, loc->tm_mon + 1, loc->tm_mday,
        loc->tm_year + 1900, loc->tm_mon + 1, loc->tm_mday,
        TagDate + 1
        );
    }

static void warn_missing_meta(char const *tag)
    {
    fprintf(stderr, "WARNING: missing meta information: %s\n", tag);
    }

static void hb_put_meta_list(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta,
    meta_item_t *def,
    char const *replace_str
    )
    {
    meta_item_t const *item;

    for(item = get_meta_or_def(meta, def, meta_tag); item != NULL; item = meta_item_next(item))
        {
        if(item != meta_item_get_unknown())
            hb_put_values(ob, out_tag, item);
        else
            {
            warn_missing_meta(meta_tag);

            if(replace_str)
                hb_put_str1_nl(ob, replace_str, out_tag);
            else
                hb_put_value_nl(ob, out_tag, item, 0);
            }
        }
    }

static void hb_put_meta_authors(
    hoedown_buffer *ob,
    char const *meta_tag,
    meta_item_t *meta,
    meta_item_t *def
    )
    {
    meta_item_t const *item;

    for(item = get_meta_or_def(meta, def, meta_tag); item != NULL; item = meta_item_next(item))
        {
        if(item == meta_item_get_unknown())
            warn_missing_meta(meta_tag);

        hb_put_author(ob, item);
        }
    }

static void hb_put_meta_no_def(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta,
    hoedown_buffer const *replace_txt
    )
    {
    meta_item_t const *item = meta_table_get(&meta, meta_tag);

    if(item)
        hb_put_value_nl(ob, out_tag, item, 0);
    else
        hb_put_text1_nl(ob, replace_txt, out_tag);
    }

static void hb_put_meta_cover(
    hoedown_buffer *ob,
    char const *meta_tag,
    meta_item_t *meta,
    hoedown_renderer *r
    )
    {
    meta_item_t const *item = meta_table_get(&meta, meta_tag);
    hoedown_buffer *link;

    if(!item)
        return;

    link = hoedown_buffer_new(TEXT_BUF_UNIT);

    hoedown_buffer_puts(link, (char *)meta_item_value(item, 0));

    hb_open_tag(ob, TagCoverPage);
      fb2_image(ob, link, NULL, NULL, (hoedown_renderer_data *)r);
    hb_close_tag_nl(ob, TagCoverPage);

    hoedown_buffer_free(link);
    }

static void hb_put_meta_value(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta,
    meta_item_t *def
    )
    {
    meta_item_t const *item = get_meta_or_def(meta, def, meta_tag);

    if(item == meta_item_get_unknown())
        warn_missing_meta(meta_tag);

    hb_put_value_nl(ob, out_tag, item, 0);
    }

static void hb_put_meta_value_str(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta,
    meta_item_t *def,
    char const *replace_str
    )
    {
    meta_item_t const *item = get_meta_or_def(meta, def, meta_tag);

    if(item != meta_item_get_unknown())
        hb_put_value_nl(ob, out_tag, item, 0);
    else
        hb_put_str1_nl(ob, replace_str, out_tag);
    }

static void hb_put_meta_date(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta
    )
    {
    meta_item_t const *item = meta_table_get(&meta, meta_tag);

    if(item)
        hb_put_value_nl(ob, out_tag, item, 0);
    else
        hb_put_current_date(ob);
    }

static void hb_put_meta_optional(
    hoedown_buffer *ob,
    char const *meta_tag,
    char const *out_tag,
    meta_item_t *meta
    )
    {
    meta_item_t const *item = meta_table_get(&meta, meta_tag);

    if(item)
        hb_put_value_nl(ob, out_tag, item, 0);
    }

static hoedown_buffer *annotation_to_fb2(unsigned char const *md_str)
    {
    hoedown_renderer *fb2;
    hoedown_document *doc;
    hoedown_buffer *body;

    body = hoedown_buffer_new(TEXT_BUF_UNIT);
    fb2 = fb2_annotation_new(NULL);
    doc = hoedown_document_new(fb2, fb2_annotation_extensions(), MAX_NESTING_ANN);

    hoedown_document_render(doc, body, md_str, strlen((char *)md_str));

    hoedown_document_free(doc);
    fb2_renderer_delete(fb2);

    return body;
    }

static void hb_put_meta_annotation(
    hoedown_buffer *ob,
    char const *meta_tag,
    meta_item_t *meta
    )
    {
    meta_item_t const *item = meta_table_get(&meta, meta_tag);

    if(item)
        {
        hoedown_buffer *ann_fb2 = annotation_to_fb2(meta_item_value(item, 0));
        hb_put_text1_nl(ob, ann_fb2, TagAnnotation);
        hoedown_buffer_free(ann_fb2);
        }
    }

static void hb_put_sequence_attrs(hoedown_buffer *ob, unsigned char const *seq_str)
    {
    size_t name_len = 0;
    size_t num_len = 0;
    unsigned char const *num_pos = (unsigned char *)strrchr((char *)seq_str, ',');

    if(num_pos != NULL)
        {
        unsigned char const *end_pos;

        name_len = num_pos - seq_str;

        ++num_pos;
        while(isspace(*num_pos))
            ++num_pos;

        end_pos = num_pos;

        while(isdigit(*end_pos))
            ++end_pos;

        num_len = end_pos - num_pos;

        while(isspace(*end_pos))
            ++end_pos;

        if(*end_pos != '\0')
            {
            num_pos = NULL;
            num_len = 0;
            }
        }

    if(num_len != 0)
        {
        while(name_len != 0 && isspace(seq_str[name_len - 1]))
            --name_len;
        }
    else
        name_len = strlen((char *)seq_str);

    if(name_len != 0)
        {
        hb_format_attr_raw(ob, AttrName, seq_str, name_len);

        if(num_len != 0)
            hb_format_attr_raw(ob, AttrNumber, num_pos, num_len);
        }
    }

static void hb_put_meta_sequence(
    hoedown_buffer *ob,
    char const *meta_tag,
    meta_item_t *meta
    )
    {
    meta_item_t const *item;

    for(item = meta_table_get(&meta, meta_tag); item != NULL; item = meta_item_next(item))
        {
        hoedown_buffer_puts(ob, TagSequence);

        if(meta_item_count_values(item) == 2)
            {
            hb_format_attrs(ob, AttrName, (char *)meta_item_value(item, 0));
            hb_format_attrs(ob, AttrNumber, (char *)meta_item_value(item, 1));
            }
        else
            hb_put_sequence_attrs(ob, meta_item_value(item, 0));

        hoedown_buffer_puts(ob, "/>\n");
        }
    }

hoedown_buffer *fb2_make_header(hoedown_renderer *r, meta_item_t **meta, meta_item_t *def)
    {
    fb2_state_t *state = r->opaque;
    hoedown_buffer *ob = hoedown_buffer_new(TEXT_BUF_UNIT);

    if(!parse_yaml(meta, state->yaml->data, state->yaml->size, 0))
        fprintf(stderr, "WARNING: Bad meta information block\n");

    parse_guess_lang(meta);

    hoedown_buffer_puts(ob, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    hoedown_buffer_puts(ob, TagFictionBook);
    hb_format_attrs(ob, AttrXmlnsXlink, "http://www.w3.org/1999/xlink");
    hb_format_attrs(ob, AttrXmlns, "http://www.gribuser.ru/xml/fictionbook/2.0");
    hoedown_buffer_puts(ob, ">\n");

    hb_open_tag(ob, TagDescription);

    /* Book info */
    hb_open_tag_nl(ob, TagTitleInfo);

    hb_put_meta_list(ob, "genre", TagGenre, *meta, def, "unrecognised");
    hb_put_meta_authors(ob, "author", *meta, def);
    hb_put_meta_no_def(ob, "title", TagBookTitle, *meta, state->book_title);
    hb_put_meta_annotation(ob, "annotation", *meta);
    hb_put_meta_optional(ob, "date", TagDate, *meta);
    hb_put_meta_cover(ob, "cover", *meta, r);
    hb_put_meta_value(ob, "lang", TagLang, *meta, def);
    hb_put_meta_sequence(ob, "series", *meta);

    hb_close_tag(ob, TagTitleInfo);

    /* FB2 info */
    hb_open_tag_nl(ob, TagDocumentInfo);

    hb_put_meta_authors(ob, "fb2-author", *meta, def);
    hb_put_str1_nl(ob, APP_NAME " " APP_SHORT_VERSION, TagProgram);
    hb_put_meta_date(ob, "fb2-date", TagDate, *meta);
    hb_put_meta_value(ob, "fb2-id", TagId, *meta, def);
    hb_put_meta_value_str(ob, "fb2-version", TagVersion, *meta, def, "1.0");

    hb_close_tag(ob, TagDocumentInfo);

    hb_close_tag_nl(ob, TagDescription);

    hb_open_tag(ob, TagBody);

    return ob;
    }

hoedown_buffer *fb2_make_footer(hoedown_renderer *r)
    {
    fb2_state_t *state = r->opaque;
    hoedown_buffer *ob = hoedown_buffer_new(TEXT_BUF_UNIT);

    hoedown_buffer_put(ob, state->binary->data, state->binary->size);
    hb_close_tag_nl(ob, TagFictionBook);

    return ob;
    }
