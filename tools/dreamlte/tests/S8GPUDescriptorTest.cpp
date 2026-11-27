#include <cassert>
#include <cstdint>

#include "../s8_mali_mmu.h"

static bool
descriptorTest(void)
{
	uint64_t table = s8gpu_mmu_table_descriptor(0x12345000ULL);
	uint64_t page = s8gpu_mmu_page_descriptor(0x12346000ULL);

	assert((table & 3ULL) == 3ULL);
	assert((table & S8GPU_MMU_PTE_AF) == 0ULL);
	assert((page & 3ULL) == 3ULL);
	assert((page & S8GPU_MMU_PTE_ADDR_MASK) == 0x12346000ULL);
	assert(((page >> 6) & 3ULL) == 1ULL);
	assert((page & S8GPU_MMU_PTE_AF) != 0ULL);
	return true;
}

static bool
mmuConfigTest(void)
{
	const uint32_t features = 0x2830u;
	const uint32_t expected = (2u << 24) | (1u << 30) | (7u << 6) | 6u;

	assert(s8gpu_mmu_geometry_supported(features));
	assert(S8GPU_MMU_FEATURES_VA_BITS(features) == 48u);
	assert(S8GPU_MMU_FEATURES_PA_BITS(features) == 40u);
	assert(s8gpu_mmu_transcfg(48u) == expected);
	assert(!s8gpu_mmu_geometry_supported(0x2730u));
	return true;
}

static bool
allocBoundsTest(void)
{
	const uint64_t vaBase = 0x02000000ULL;
	const uint64_t vaEnd = 0x100000000ULL;
	const uint64_t pageMask = 4095ULL;

	assert(((4096ULL + pageMask) & ~pageMask) == 4096ULL);
	assert(((4097ULL + pageMask) & ~pageMask) == 8192ULL);
	assert(vaBase + 4096ULL < vaEnd);
	assert(vaEnd - vaBase > 4096ULL);
	return true;
}

int
main(void)
{
	assert(descriptorTest());
	assert(mmuConfigTest());
	assert(allocBoundsTest());
	return 0;
}
