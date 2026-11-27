/*
 * S8MaliMetal libraries and functions.
 *
 * The driver keeps its own library objects: it reads the metallib's
 * function list (names and types; tools/dreamlte/metal/metallib.py shows
 * the format) and records which function and which function-constant
 * values each request names. Shaders themselves are compiled ahead of
 * time for the Mali (stage 3), keyed by exactly these requests.
 */
#import "s8mtl.h"

int open(const char *, int, ...);
int close(int);
long read(int, void *, unsigned long);
long long lseek(int, long long, int);
void *malloc(size_t);
void free(void *);
int strcmp(const char *, const char *);
size_t strlen(const char *);
int snprintf(char *, size_t, const char *, ...);

static uint8_t *s8_read_file(const char *path, size_t *len)
{
	int fd = open(path, 0);
	if (fd < 0)
		return NULL;
	long long size = lseek(fd, 0, 2);
	lseek(fd, 0, 0);
	uint8_t *b = size > 0 ? malloc((size_t)size) : NULL;
	size_t got = 0;
	while (b != NULL && got < (size_t)size) {
		long n = read(fd, b + got, (size_t)size - got);
		if (n <= 0)
			break;
		got += (size_t)n;
	}
	close(fd);
	if (b != NULL && got != (size_t)size) {
		free(b);
		return NULL;
	}
	*len = got;
	return b;
}

@interface S8Library ()
- (BOOL)s8_parseBytes:(const uint8_t *)data length:(size_t)length;
@end

/* Function-constant values as "name=0xbits;..." in the order Metal keeps
 * them (MTLFunctionConstantValuesInternal -newNamedConstantArray, elements
 * MTLNamedConstantValue name/dataType/data). Values are the constant's
 * bytes, little endian, as air2nir -c takes them. */
static unsigned s8_type_size(NSUInteger t)
{
	if (t >= 3 && t <= 6) return 4 * (unsigned)(t - 2);          /* float..float4 */
	if (t >= 16 && t <= 19) return 2 * (unsigned)(t - 15);        /* half..half4 */
	if (t >= 29 && t <= 36) return 4 * (unsigned)((t - 29) % 4 + 1); /* int, uint */
	if (t >= 37 && t <= 44) return 2 * (unsigned)((t - 37) % 4 + 1); /* short, ushort */
	if (t >= 45 && t <= 56) return (unsigned)((t - 45) % 4 + 1);  /* char, uchar, bool */
	return 0;
}

static void s8_constants_key(id values, char *out, size_t len)
{
	size_t n = 0;
	out[0] = 0;
	id arr = [values newNamedConstantArray];
	NSUInteger count = [arr count];
	for (NSUInteger i = 0; i < count; i++) {
		id v = [arr objectAtIndex:i];
		const char *name = [[v name] UTF8String];
		NSUInteger type = [v dataType];
		const uint8_t *d = [v data];
		unsigned sz = s8_type_size(type);
		uint64_t bits = 0;
		for (unsigned k = 0; k < sz && k < 8 && d != NULL; k++)
			bits |= (uint64_t)d[k] << (8 * k);
		int w = snprintf(out + n, len - n, "%s%s=0x%llx", n ? ";" : "", name ? name : "?",
		    (unsigned long long)bits);
		if (w < 0 || (size_t)w >= len - n) {
			out[n] = 0;
			break;
		}
		n += (size_t)w;
	}
	[arr release];
}

@implementation S8Library

- (instancetype)initWithDevice:(id)device path:(const char *)path
{
	self = [super init];
	if (self == nil)
		return nil;
	_s8device = device;
	size_t len = 0;
	uint8_t *d = path ? s8_read_file(path, &len) : NULL;
	BOOL valid = d != NULL && [self s8_parseBytes:d length:len];
	free(d);
	if (!valid) {
		static unsigned rejected;
		if (rejected++ < 8)
			s8_log("MTL library rejected %s (%lu bytes)", path ? path : "?",
			    (unsigned long)len);
		[self release];
		return nil;
	}
	s8_log("MTL library %s: %u functions", path ? path : "?", _nfuncs);
	return self;
}

- (instancetype)initWithDevice:(id)device bytes:(const void *)bytes length:(size_t)length
{
	self = [super init];
	if (self == nil)
		return nil;
	_s8device = device;
	if (![self s8_parseBytes:bytes length:length]) {
		[self release];
		return nil;
	}
	s8_log("MTL library data: %u functions", _nfuncs);
	return self;
}

- (BOOL)s8_parseBytes:(const uint8_t *)data length:(size_t)length
{
	return s8_mtl_parse(data, length, &_funcs, &_nfuncs) ? YES : NO;
}

- (void)dealloc
{
	free(_funcs);
	[super dealloc];
}

S8_FORWARDING

- (id)device { return _s8device; }
- (NSUInteger)type { return 0; }

/* metallib TYPE: 0 vertex, 1 fragment, 2 kernel -> MTLFunctionType. */
- (S8Function *)s8_function:(id)name constants:(id)constants
{
	const char *n = [name UTF8String];
	unsigned i;
	for (i = 0; i < _nfuncs && n != NULL; i++)
		if (strcmp(_funcs[i].name, n) == 0)
			break;
	if (n == NULL || i == _nfuncs) {
		s8_log("MTL library: no function %s", n ? n : "(null)");
		return nil;
	}
	S8Function *f = [[S8Function alloc] init];
	f->_s8device = _s8device;
	f->_name = [name retain];
	f->_type = _funcs[i].type == 0 ? MTLFunctionTypeVertex :
	    _funcs[i].type == 1 ? MTLFunctionTypeFragment : MTLFunctionTypeKernel;
	if (constants != nil)
		s8_constants_key(constants, f->_constants, sizeof(f->_constants));
	return f;
}

- (id)newFunctionWithName:(id)name
{
	return [self s8_function:name constants:nil];
}

- (id)newFunctionWithName:(id)name constantValues:(id)constants error:(id *)error
{
	if (error != NULL)
		*error = nil;
	return [self s8_function:name constants:constants];
}

- (id)newFunctionWithDescriptor:(id)desc error:(id *)error
{
	if (error != NULL)
		*error = nil;
	return [self s8_function:[desc name] constants:[desc constantValues]];
}

@end

@implementation S8Function

- (void)dealloc
{
	[_name release];
	[super dealloc];
}

S8_FORWARDING

- (id)device { return _s8device; }
- (id)name { return _name; }
- (NSUInteger)functionType { return _type; }
- (NSUInteger)options { return 0; }
- (id)vertexAttributes { return nil; }
- (id)stageInputAttributes { return nil; }
- (NSInteger)patchType { return 0; }
- (NSInteger)patchControlPointCount { return -1; }

@end
