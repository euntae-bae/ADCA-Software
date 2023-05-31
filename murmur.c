#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static inline uint32_t murmur_32_scramble(uint32_t k) {
	k *= 0xcc9e2d51;
	k = (k << 15) | (k >> 17);
	k *= 0x1b873593;
	return k;
}
uint32_t murmur3_32(const uint8_t* key, size_t len)
{
	uint32_t k = 0;
    uint32_t h = 1000; // fixed seed
    size_t remByte = len;

	/* Read in groups of 4. */
	while (remByte >= 4) {
		memcpy(&k, key, sizeof(uint32_t));
		key += sizeof(uint32_t);
		h ^= murmur_32_scramble(k);
		h = (h << 13) | (h >> 19);
		h = h * 5 + 0xe6546b64;
        key += sizeof(uint32_t);
        remByte -= 4;
    }

	/* Read the rest. */
	k = 0;
	for (size_t i = len & 3; i; i--) {
		k <<= 8;
		k |= key[i - 1];
	}
	// A swap is *not* necessary here because the preceding loop already
	// places the low bytes in the low places according to whatever endianness
	// we use. Swaps only apply when the memory is copied in a chunk.
	h ^= murmur_32_scramble(k);
	/* Finalize. */
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

