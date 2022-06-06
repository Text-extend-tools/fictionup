/*
Copyright 2017-2019 Vadim Druzhin <cdslow@mail.ru>

You can redistribute and/or modify this file under the terms
of the GNU General Public License version 3 or any later version.

See file COPYING or visit <http://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE /* memrchr */
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include "buffer.h"
#include "version.h"
#include "utils.h"

#define PATH_UNIT 64
#define BUF_UNIT 65536

void const *mem_last_path_part(void const *path, size_t n)
    {
    void const *found = memrchr(path, '/', n);

    if(found == NULL)
        found = memrchr(path, '\\', n);

    if(found == NULL)
        found = path;
    else
        found = (char const *)found + 1;

    return found;
    }

char const *str_last_path_part(char const *path)
    {
    return mem_last_path_part(path, strlen(path));
    }

void add_path_separator(hoedown_buffer *buf)
    {
    if(buf->size == 0)
        return;

    if(is_path_separator(buf->data[buf->size - 1]))
        return;

    hoedown_buffer_putc(buf, '/');
    }

void append_path_data(hoedown_buffer *path1, uint8_t const *data2, size_t size2)
    {
    if(path1->size == 0)
        {
        hoedown_buffer_set(path1, data2, size2);
        return;
        }

    add_path_separator(path1);

    if(size2 == 0)
        return;

    if(is_path_separator(data2[0]))
        hoedown_buffer_put(path1, data2 + 1, size2 - 1);
    else
        hoedown_buffer_put(path1, data2, size2);
    }

void append_path_str(hoedown_buffer *path1, char const *str2)
    {
    append_path_data(path1, (uint8_t const *)str2, strlen(str2));
    }

void concat_path_data(
    hoedown_buffer *full_path, hoedown_buffer const *path1, uint8_t const *data2, size_t size2)
    {
    hoedown_buffer_set(full_path, path1->data, path1->size);
    append_path_data(full_path, data2, size2);
    }

void concat_path_str(
    hoedown_buffer *full_path, hoedown_buffer const *path1, char const *str2)
    {
    concat_path_data(full_path, path1, (uint8_t const *)str2, strlen(str2));
    }

int is_directory(char const *path)
    {
    struct stat st;

    if(stat(path, &st) != 0)
        return 0;

    return S_ISDIR(st.st_mode);
    }

hoedown_buffer *get_exe_path(char const *argv0)
    {
    hoedown_buffer *path = hoedown_buffer_new(PATH_UNIT);
    ssize_t len = PATH_UNIT;
    uint8_t const *name;

    do
        {
        hoedown_buffer_grow(path, len);
        len = readlink("/proc/self/exe", (char *)path->data, path->size);
        }
    while(len > (ssize_t)path->size);

    if(len == -1)
        {
        char *rpath = realpath(argv0, NULL);

        if(rpath == NULL)
            {
            path->size = 0;
            return path;
            }

        hoedown_buffer_sets(path, rpath);
        free(rpath);
        }
    else
        path->size = len;

    name = mem_last_path_part(path->data, path->size);
    path->size = name - path->data;

    return path;
    }

hoedown_buffer *get_data_path(char const *argv0)
    {
    hoedown_buffer *path = get_exe_path(argv0);
    size_t base_size = path->size;;
    char const *dir;

    dir = "../share/" EXE_NAME;
    append_path_str(path, dir);

    if(is_directory(hoedown_buffer_cstr(path)))
        return path;

    dir = "../usr/share/" EXE_NAME;
    path->size = base_size;
    append_path_str(path, dir);

    if(is_directory(hoedown_buffer_cstr(path)))
        return path;

    path->size = base_size;

    return path;
    }

hoedown_buffer *get_user_data_path(void)
    {
    hoedown_buffer *path = hoedown_buffer_new(PATH_UNIT);
    char const *data_dir = EXE_NAME;
    char *config_dir;

    config_dir = getenv("XDG_CONFIG_HOME");

    if(config_dir == NULL || *config_dir == '\0')
        {
        data_dir = ".config/" EXE_NAME;
        config_dir = getenv("HOME");
        }

    if(config_dir == NULL || *config_dir == '\0')
        return NULL;

    hoedown_buffer_sets(path, config_dir);
    append_path_str(path, data_dir);

    return path;
    }

void list_files(glob_t *list, hoedown_buffer const *dir, char const *mask)
    {
    hoedown_buffer *pattern = hoedown_buffer_new(PATH_UNIT);

    concat_path_str(pattern, dir, mask);
    glob(hoedown_buffer_cstr(pattern), 0, NULL, list);
    hoedown_buffer_free(pattern);
    }

void list_files_free(glob_t *list)
    {
    globfree(list);
    }

hoedown_buffer *read_file(FILE *in, int required)
    {
    hoedown_buffer *buf = hoedown_buffer_new(BUF_UNIT);

    if(hoedown_buffer_putf(buf, in) != 0)
        {
        char const *severity = required ? "ERROR" : "WARNING";

        fprintf(stderr, "%s: Read failed!\n", severity);
        perror(severity);

        if(required)
            abort();
        else
            {
            hoedown_buffer_free(buf);
            buf = NULL;
            }
        }

    return buf;
    }

char const *str_ends_with(char const *str, char const *end)
    {
    size_t end_len = strlen(end);
    size_t str_len = strlen(str);

    if(str_len < end_len)
        return NULL;

    str += str_len - end_len;

    if(strcasecmp(str, end) != 0)
       return NULL;

    return str;
    }

