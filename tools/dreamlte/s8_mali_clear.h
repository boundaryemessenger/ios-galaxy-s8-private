#ifndef S8_MALI_CLEAR_H
#define S8_MALI_CLEAR_H

#include <stdint.h>

#define S8GPU_CLEAR256_SIZE 256u
#define S8GPU_CLEAR256_STRIDE (S8GPU_CLEAR256_SIZE * 4u)
#define S8GPU_CLEAR256_COLOR 0xffc0c0c0u

static inline void
s8gpu_clear256_build(volatile uint32_t *job, volatile uint32_t *fb,
    uint64_t fb_gpu_va, uint64_t dst_gpu_va, uint32_t dst_stride)
{
	for (unsigned i = 0; i < 16; i++) {
		job[i] = 0;
	}
	for (unsigned i = 0; i < 128; i++) {
		fb[i] = 0;
	}

	fb[0] = 128u | (128u << 16);
	fb[0x80 / 4] = 128u | (128u << 16);
	unsigned fbd = 0x100 / 4;
	unsigned rt = fbd + 128 / 4;
	fb[fbd + 4] = (uint32_t)fb_gpu_va;
	fb[fbd + 5] = (uint32_t)(fb_gpu_va >> 32);
	fb[fbd + 8] = (S8GPU_CLEAR256_SIZE - 1) |
	    ((S8GPU_CLEAR256_SIZE - 1) << 16);
	fb[fbd + 10] = (S8GPU_CLEAR256_SIZE - 1) |
	    ((S8GPU_CLEAR256_SIZE - 1) << 16);
	fb[fbd + 11] = (8u << 9) | (1u << 24);
	fb[rt] = 1u << 26;
	fb[rt + 1] = 1u | (19u << 3) | (2u << 10) | (0x688u << 16) | (1u << 31);
	fb[rt + 8] = (uint32_t)dst_gpu_va;
	fb[rt + 9] = (uint32_t)(dst_gpu_va >> 32);
	fb[rt + 10] = dst_stride;
	for (unsigned c = 0; c < 4; c++) {
		fb[rt + 12 + c] = S8GPU_CLEAR256_COLOR;
	}

	job[4] = 1u | (9u << 1) | (1u << 16);
	job[9] = (S8GPU_CLEAR256_SIZE / 16 - 1) |
	    ((S8GPU_CLEAR256_SIZE / 16 - 1) << 16);
	uint64_t fbd_gpu_va = (fb_gpu_va + 0x100) | 1u;
	job[10] = (uint32_t)fbd_gpu_va;
	job[11] = (uint32_t)(fbd_gpu_va >> 32);
}

#endif
