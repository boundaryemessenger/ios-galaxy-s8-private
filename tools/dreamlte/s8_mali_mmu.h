#ifndef S8_MALI_MMU_H
#define S8_MALI_MMU_H

#include <stdbool.h>
#include <stdint.h>

#define S8GPU_MMU_FEATURES_VA_BITS(features) ((uint32_t)(features) & 0xffu)
#define S8GPU_MMU_FEATURES_PA_BITS(features) (((uint32_t)(features) >> 8) & 0xffu)
#define S8GPU_MMU_REQUIRED_VA_BITS 48u
#define S8GPU_MMU_REQUIRED_PA_BITS 40u

#define S8GPU_MMU_TRANSCFG_ADRMODE_AARCH64_4K 6u
#define S8GPU_MMU_TRANSCFG_INA_BITS(bits) ((uint32_t)(bits) << 6)
#define S8GPU_MMU_TRANSCFG_PTW_MEMATTR_WB (2u << 24)
#define S8GPU_MMU_TRANSCFG_PTW_RA (1u << 30)

#define S8GPU_MMU_PTE_ADDR_MASK 0x0000fffffffff000ULL
#define S8GPU_MMU_PTE_TABLE 3ULL
#define S8GPU_MMU_PTE_PAGE 3ULL
#define S8GPU_MMU_PTE_AF (1ULL << 10)
#define S8GPU_MMU_PTE_AP_UNPRIV (1ULL << 6)

static inline bool
s8gpu_mmu_geometry_supported(uint32_t features)
{
	return S8GPU_MMU_FEATURES_VA_BITS(features) == S8GPU_MMU_REQUIRED_VA_BITS &&
	    S8GPU_MMU_FEATURES_PA_BITS(features) == S8GPU_MMU_REQUIRED_PA_BITS;
}

static inline uint32_t
s8gpu_mmu_transcfg(uint32_t va_bits)
{
	return S8GPU_MMU_TRANSCFG_PTW_MEMATTR_WB |
	    S8GPU_MMU_TRANSCFG_PTW_RA |
	    S8GPU_MMU_TRANSCFG_INA_BITS(55u - va_bits) |
	    S8GPU_MMU_TRANSCFG_ADRMODE_AARCH64_4K;
}

static inline uint64_t
s8gpu_mmu_table_descriptor(uint64_t pa)
{
	return (pa & S8GPU_MMU_PTE_ADDR_MASK) | S8GPU_MMU_PTE_TABLE;
}

static inline uint64_t
s8gpu_mmu_page_descriptor(uint64_t pa)
{
	return (pa & S8GPU_MMU_PTE_ADDR_MASK) | S8GPU_MMU_PTE_AP_UNPRIV |
	    S8GPU_MMU_PTE_AF | S8GPU_MMU_PTE_PAGE;
}

#endif
