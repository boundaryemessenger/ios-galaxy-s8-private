#include "s8mtl_metallib.h"

void *calloc(size_t, size_t);
void *realloc(void *, size_t);
void free(void *);
void *memcpy(void *, const void *, size_t);
int strcmp(const char *, const char *);

static uint16_t s8_le16(const uint8_t *p)
{
	return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

static uint32_t s8_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint64_t s8_le64(const uint8_t *p)
{
	return (uint64_t)s8_le32(p) | (uint64_t)s8_le32(p + 4) << 32;
}

static uint32_t s8_be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint64_t s8_be64(const uint8_t *p)
{
	return (uint64_t)s8_be32(p) << 32 | s8_be32(p + 4);
}

static int s8_parse_mtlb(const uint8_t *data, size_t length,
    struct s8_mtl_function **functions, unsigned *count)
{
	if (data == NULL || length < 0x58 || s8_le32(data) != 0x424c544d)
		return 0;
	uint64_t list = s8_le64(data + 0x18);
	if (list > length || length - (size_t)list < 4)
		return 0;
	uint32_t nfuncs = s8_le32(data + list);
	if (nfuncs > 65536 || nfuncs > (length - (size_t)list - 4) / 4)
		return 0;
	struct s8_mtl_function *parsed = calloc(nfuncs ? nfuncs : 1, sizeof(*parsed));
	if (parsed == NULL)
		return 0;

	uint64_t off = list + 4;
	unsigned valid = 0;
	for (uint32_t i = 0; i < nfuncs; i++) {
		if (off > length || length - (size_t)off < 4)
			goto fail;
		uint32_t flen = s8_le32(data + off);
		if (flen < 4 || flen > length - (size_t)off)
			goto fail;
		uint64_t p = off + 4, end = off + flen;
		struct s8_mtl_function function = { { 0 }, 0 };
		int has_name = 0, has_type = 0, usable = 1;
		while (p < end) {
			if (end - p < 4)
				goto fail;
			uint32_t tag = s8_le32(data + p);
			if (tag == 0x54444e45)
				break;
			if (end - p < 6)
				goto fail;
			uint16_t size = s8_le16(data + p + 4);
			p += 6;
			if (size > end - p)
				goto fail;
			if (tag == 0x454d414e) {
				if (size == 0 || size >= sizeof(function.name)) {
					/* Shader-cache keys are limited to 63 name bytes. */
					usable = 0;
				} else {
					memcpy(function.name, data + p, size);
					function.name[size] = '\0';
					has_name = 1;
				}
			} else if (tag == 0x45505954) {
				if (size == 0) {
					usable = 0;
				} else {
					function.type = data[p];
					has_type = 1;
				}
			}
			p += size;
		}
		if (usable && has_name && has_type)
			parsed[valid++] = function;
		off = end;
	}
	*functions = parsed;
	*count = valid;
	return 1;

fail:
	free(parsed);
	return 0;
}

static int s8_append_unique(struct s8_mtl_function **all, unsigned *all_count,
    const struct s8_mtl_function *part, unsigned part_count)
{
	for (unsigned i = 0; i < part_count; i++) {
		unsigned j;
		for (j = 0; j < *all_count; j++)
			if (strcmp((*all)[j].name, part[i].name) == 0)
				break;
		if (j != *all_count)
			continue;
		if (*all_count >= 65536)
			return 0;
		struct s8_mtl_function *grown = realloc(*all, (*all_count + 1) * sizeof(**all));
		if (grown == NULL)
			return 0;
		*all = grown;
		(*all)[(*all_count)++] = part[i];
	}
	return 1;
}

static uint32_t s8_fat32(const uint8_t *p, int little)
{
	return little ? s8_le32(p) : s8_be32(p);
}

static uint64_t s8_fat64(const uint8_t *p, int little)
{
	return little ? s8_le64(p) : s8_be64(p);
}

int s8_mtl_parse(const void *input, size_t length,
    struct s8_mtl_function **functions, unsigned *count)
{
	if (functions == NULL || count == NULL)
		return 0;
	*functions = NULL;
	*count = 0;
	const uint8_t *data = input;
	if (data == NULL)
		return 0;
	if (length >= 4 && s8_le32(data) == 0x424c544d)
		return s8_parse_mtlb(data, length, functions, count);
	if (length < 8)
		return 0;

	uint32_t magic = s8_be32(data);
	int little, wide;
	if (magic == 0xcafebabe) {
		little = 0;
		wide = 0;
	} else if (magic == 0xbebafeca) {
		little = 1;
		wide = 0;
	} else if (magic == 0xcafebabf) {
		little = 0;
		wide = 1;
	} else if (magic == 0xbfbafeca) {
		little = 1;
		wide = 1;
	} else {
		return 0;
	}

	uint32_t narch = s8_fat32(data + 4, little);
	size_t arch_size = wide ? 32 : 20;
	if (narch == 0 || narch > 64 || narch > (length - 8) / arch_size)
		return 0;
	struct s8_mtl_function *all = NULL;
	unsigned all_count = 0;
	unsigned slices = 0;
	/* Function lookup needs metadata only, so merge names from every slice. */
	for (uint32_t i = 0; i < narch; i++) {
		const uint8_t *arch = data + 8 + (size_t)i * arch_size;
		uint64_t offset = wide ? s8_fat64(arch + 8, little) : s8_fat32(arch + 8, little);
		uint64_t size = wide ? s8_fat64(arch + 16, little) : s8_fat32(arch + 12, little);
		if (offset > length || size > length - (size_t)offset || size < 4 ||
		    s8_le32(data + (size_t)offset) != 0x424c544d)
			continue;
		struct s8_mtl_function *part = NULL;
		unsigned part_count = 0;
		if (!s8_parse_mtlb(data + (size_t)offset, (size_t)size, &part, &part_count))
			continue;
		slices++;
		int ok = s8_append_unique(&all, &all_count, part, part_count);
		free(part);
		if (!ok) {
			free(all);
			return 0;
		}
	}
	if (slices == 0) {
		free(all);
		return 0;
	}
	*functions = all;
	*count = all_count;
	return 1;
}
