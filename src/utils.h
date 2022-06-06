#pragma once
#include <stdint.h>
#include <glob.h>

static inline int is_path_separator(char c)
    {
    return c == '/' || c == '\\';
    }

void const *mem_last_path_part(void const *path, size_t n);
char const *str_last_path_part(char const *path);
void add_path_separator(hoedown_buffer *buf);
void append_path_data(hoedown_buffer *path1, uint8_t const *data2, size_t size2);
void append_path_str(hoedown_buffer *path1, char const *str2);
void concat_path_data(
    hoedown_buffer *full_path, hoedown_buffer const *path1, uint8_t const *data2, size_t size2);
void concat_path_str(
    hoedown_buffer *full_path, hoedown_buffer const *path1, char const *str2);
int is_directory(char const *path);
hoedown_buffer *get_exe_path(char const *argv0);
hoedown_buffer *get_data_path(char const *argv0);
hoedown_buffer *get_user_data_path(void);
void list_files(glob_t *list, hoedown_buffer const *dir, char const *mask);
void list_files_free(glob_t *list);
hoedown_buffer *read_file(FILE *in, int required);
char const *str_ends_with(char const *str, char const *end);
