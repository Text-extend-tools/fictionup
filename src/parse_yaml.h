#pragma once
#include <stdint.h>
#include "meta.h"

int parse_yaml(meta_item_t **table, uint8_t const *data, size_t size, int def);
void parse_guess_lang(meta_item_t **table);

