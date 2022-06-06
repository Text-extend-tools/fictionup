#pragma once

void meta_subst_init(void);
void meta_subst_fini(void);
void meta_subst_add(char const *key, char const *value, char const *lang);
void meta_subst_load_system(char const *argv0);
void meta_subst_load_user(void);
char const *meta_subst(char const *key);
char const *meta_subst_def(char const *key);
char const *meta_subst_get_lang();
