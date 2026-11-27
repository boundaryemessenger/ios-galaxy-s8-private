#pragma once
#include <stddef.h>
#include <stdint.h>

struct s8_mtl_function {
	char name[64];
	uint8_t type;
};

int s8_mtl_parse(const void *data, size_t length,
    struct s8_mtl_function **functions, unsigned *count);
