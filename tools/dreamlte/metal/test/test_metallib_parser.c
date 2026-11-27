#include "../s8mtl_metallib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct blob {
	uint8_t data[4096];
	size_t size;
};

static void put16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static void put64(uint8_t *p, uint64_t v)
{
	put32(p, (uint32_t)v);
	put32(p + 4, (uint32_t)(v >> 32));
}

static void putbe32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void field(uint8_t *p, size_t *n, const char *tag, const void *value, uint16_t size)
{
	memcpy(p + *n, tag, 4);
	put16(p + *n + 4, size);
	memcpy(p + *n + 6, value, size);
	*n += 6 + size;
}

static void function(uint8_t *p, size_t *n, const char *name, uint8_t type)
{
	size_t start = *n;
	*n += 4;
	field(p, n, "NAME", name, (uint16_t)strlen(name));
	field(p, n, "TYPE", &type, 1);
	memcpy(p + *n, "ENDT", 4);
	*n += 4;
	put32(p + start, (uint32_t)(*n - start));
}

static struct blob make_mtlb(const char **names, const uint8_t *types, unsigned count)
{
	struct blob b = { { 0 }, 0x58 };
	memcpy(b.data, "MTLB", 4);
	put64(b.data + 0x18, 0x58);
	put32(b.data + 0x58, count);
	b.size += 4;
	for (unsigned i = 0; i < count; i++)
		function(b.data, &b.size, names[i], types[i]);
	return b;
}

static int has_function(const struct s8_mtl_function *functions, unsigned count,
    const char *name, uint8_t type)
{
	for (unsigned i = 0; i < count; i++)
		if (strcmp(functions[i].name, name) == 0 && functions[i].type == type)
			return 1;
	return 0;
}

int main(int argc, char **argv)
{
	char long_name[81];
	memset(long_name, 'x', sizeof(long_name) - 1);
	long_name[sizeof(long_name) - 1] = '\0';
	const char *first_names[] = { "QuartzCoreVertex", long_name, "QuartzCoreFragment" };
	const uint8_t first_types[] = { 0, 1, 1 };
	struct blob first = make_mtlb(first_names, first_types, 3);
	const char *second_names[] = { "QuartzCoreVertex", "CoreImageFunction" };
	const uint8_t second_types[] = { 0, 2 };
	struct blob second = make_mtlb(second_names, second_types, 2);

	struct s8_mtl_function *functions = NULL;
	unsigned count = 0;
	int parsed = s8_mtl_parse(first.data, first.size, &functions, &count);
	if (!parsed || count != 2 ||
	    !has_function(functions, count, "QuartzCoreVertex", 0) ||
	    !has_function(functions, count, "QuartzCoreFragment", 1)) {
		fprintf(stderr, "thin metallib parse failed (parsed=%d count=%u size=%zu)\n",
		    parsed, count, first.size);
		free(functions);
		return 1;
	}
	free(functions);

	struct blob fat = { { 0 }, 48 };
	size_t second_offset = (fat.size + first.size + 7) & ~(size_t)7;
	fat.size = second_offset + second.size;
	memcpy(fat.data, "\xca\xfe\xba\xbe", 4);
	putbe32(fat.data + 4, 2);
	putbe32(fat.data + 8, 0x01000017);
	putbe32(fat.data + 12, 8);
	putbe32(fat.data + 16, 48);
	putbe32(fat.data + 20, (uint32_t)first.size);
	putbe32(fat.data + 24, 3);
	putbe32(fat.data + 28, 0x01000017);
	putbe32(fat.data + 32, 13);
	putbe32(fat.data + 36, (uint32_t)second_offset);
	putbe32(fat.data + 40, (uint32_t)second.size);
	putbe32(fat.data + 44, 3);
	memcpy(fat.data + 48, first.data, first.size);
	memcpy(fat.data + second_offset, second.data, second.size);

	functions = NULL;
	count = 0;
	if (!s8_mtl_parse(fat.data, fat.size, &functions, &count) || count != 3 ||
	    !has_function(functions, count, "QuartzCoreVertex", 0) ||
	    !has_function(functions, count, "QuartzCoreFragment", 1) ||
	    !has_function(functions, count, "CoreImageFunction", 2)) {
		fprintf(stderr, "fat metallib parse/merge failed (count=%u)\n", count);
		free(functions);
		return 1;
	}
	free(functions);
	if (argc > 1) {
		FILE *file = fopen(argv[1], "rb");
		if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
			fprintf(stderr, "could not open test metallib\n");
			if (file != NULL)
				fclose(file);
			return 1;
		}
		long file_size = ftell(file);
		if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
			fclose(file);
			fprintf(stderr, "could not size test metallib\n");
			return 1;
		}
		uint8_t *file_data = malloc((size_t)file_size);
		if (file_data == NULL || fread(file_data, 1, (size_t)file_size, file) != (size_t)file_size) {
			free(file_data);
			fclose(file);
			fprintf(stderr, "could not read test metallib\n");
			return 1;
		}
		fclose(file);
		functions = NULL;
		count = 0;
		int ok = s8_mtl_parse(file_data, (size_t)file_size, &functions, &count);
		free(file_data);
		free(functions);
		if (!ok || count == 0) {
			fprintf(stderr, "real metallib parse failed (count=%u)\n", count);
			return 1;
		}
		printf("real metallib parse passed (%u usable functions)\n", count);
	}
	puts("thin/fat metallib parsing passed; unsupported long-name function skipped");
	return 0;
}
