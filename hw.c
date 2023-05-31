//#include <stdio.h>
#include <uart.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define bit_set(v,n)	((v)[(n) >> 3] |= (0x1 << (0x7 - ((n) & 0x7))))
#define bit_get(v,n)	((v)[(n) >> 3] &  (0x1 << (0x7 - ((n) & 0x7))))
#define bit_clr(v,n)	((v)[(n) >> 3] &=~(0x1 << (0x7 - ((n) & 0x7))))

#define WKSIZE 10000
#define BLMNUM 1000
#define INITVAL 0xdeadbeea

static unsigned char bloom_filter[BLMNUM][1];

#define __always_inline                 inline __attribute__((__always_inline__))

static __always_inline uint64_t rdcycle(void) {
    uint64_t n;
    asm volatile ( "rdcycle %0" : "=r"(n) );
    return n;
}

#define asm_murmur3_step    asm volatile ("murmur3.step")
#define asm_jhash_step      asm volatile ("jhash.step")

static __always_inline void murmur3_init(uint32_t seed, size_t len) {
    asm volatile (
        "murmur3.init %[seed], %[len]"
        : : [seed]"r"(seed), [len]"r"(len)
    );
}

static __always_inline void murmur3_load(uint32_t chunk, size_t bytesize) {
    asm volatile (
        "murmur3.load %[chunk], %[bytesize]"
        : : [chunk]"r"(chunk), [bytesize]"r"(bytesize)
    );
}

static __always_inline void murmur3_step(void) {
    asm volatile ("murmur3.step");
}

static __always_inline uint32_t murmur3_hash(void) {
    uint32_t h = 0;
    asm volatile (
//        "nop\n\t"
        "murmur3.hash %[h]"
        : [h]"=r"(h)
    );
    return h;
}

static __always_inline void jhash_init(uint32_t iv, size_t len) {
    asm volatile (
        "jhash.init %[iv], %[len]"
        : : [iv]"r"(iv), [len]"r"(len)
    );
}

static __always_inline void jhash_loadv0(uint32_t chunk) {
    asm volatile (
        "jhash.loadv0 %0"
        : : "r"(chunk)
    );
}

static __always_inline void jhash_loadv1(uint32_t chunk) {
    asm volatile (
        "jhash.loadv1 %0"
        : : "r"(chunk)
    );
}

static __always_inline void jhash_loadv2(uint32_t chunk) {
    asm volatile (
        "jhash.loadv2 %0"
        : : "r"(chunk)
    );
}

static __always_inline void jhash_step(void) {
    asm volatile ("jhash.step");
}

static __always_inline uint32_t jhash_hash(void) {
    uint32_t h = 0;
    asm volatile (
        "jhash.hash %0"
        : "=r"(h)
    );
    return h;
}

uint32_t murmur3_hw(const uint8_t *msgaddr, size_t msglen, uint32_t seed) {
    uint32_t chunk = 0;
    uint32_t hash = 0;
    uint32_t chunksz = 0;
    size_t remByte = msglen;

    murmur3_init(seed, msglen);

    // 4바이트 단위로 메시지를 읽어서 처리
    while (remByte >= 4) {
        memcpy(&chunk, msgaddr, sizeof(uint32_t));
        murmur3_load(chunk, 4);
        murmur3_step();
        msgaddr += sizeof(uint32_t);
        remByte -= 4;
    }

    // 나머지 바이트 처리
    // 빅엔디안 순서로 4바이트를 채운다.
    // 가속기에서는 이 빅엔디안 청크를 다시 리틀 엔디안으로 변환하여 처리한다.
    int i;
    uint8_t rdbyte = 0;
    chunk = 0;
    for (i = 0; i < 4; i++) {
        rdbyte = 0;
        if (remByte > 0)
            rdbyte = *msgaddr;
        chunk <<= 8;
        chunk |= rdbyte;
        msgaddr++;
        remByte--;
    }
    // Finalize
    murmur3_load(chunk, (msglen & 3));
    murmur3_step();
    hash = murmur3_hash();
    return hash;
}

uint32_t jhash_hw(const uint8_t *msgaddr, size_t msglen, uint32_t initval) {
    uint32_t hash;
    uint32_t memRead[3];
    size_t remByte = msglen;

    jhash_init(initval, msglen);
    while (remByte > 12) {
        memRead[0] = *(uint32_t*)msgaddr;
        memRead[1] = *(uint32_t*)(msgaddr + 4);
        memRead[2] = *(uint32_t*)(msgaddr + 8);

        jhash_loadv0(memRead[0]);
        jhash_loadv1(memRead[1]);
        jhash_loadv2(memRead[2]);
        jhash_step();

        remByte -= 12;
        msgaddr += 12;
    }

    memRead[0] = memRead[1] = memRead[2] = 0;
    switch (remByte) {
    case 12: memRead[2] += (uint32_t)msgaddr[11] << 24;
	case 11: memRead[2] += (uint32_t)msgaddr[10] << 16;
	case 10: memRead[2] += (uint32_t)msgaddr[9] << 8;
	case 9:  memRead[2] += msgaddr[8];
	case 8:  memRead[1] += (uint32_t)msgaddr[7] << 24;
	case 7:  memRead[1] += (uint32_t)msgaddr[6] << 16;
	case 6:  memRead[1] += (uint32_t)msgaddr[5] << 8;
	case 5:  memRead[1] += msgaddr[4];
	case 4:  memRead[0] += (uint32_t)msgaddr[3] << 24;
	case 3:  memRead[0] += (uint32_t)msgaddr[2] << 16;
	case 2:  memRead[0] += (uint32_t)msgaddr[1] << 8;
	case 1:
        memRead[0] += msgaddr[0];
        jhash_loadv0(memRead[0]);
        jhash_loadv1(memRead[1]);
        jhash_loadv2(memRead[2]);
        asm_jhash_step;
		break;
	case 0: /* Nothing left to add */
		break;
    }

    hash = jhash_hash();
    return hash;
}

void bloom_init(){
	memset(bloom_filter, 0, sizeof(unsigned char)*BLMNUM);
}

void key_generator(uint8_t *key, int key_length){
	static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	
	for  (int i = 0; i < key_length; i++){
		int tmp_key = rand() % (int)(sizeof(charset) -1);
		key[i] = charset[tmp_key];
	}
	key[key_length] = '\0';
}

uint64_t bloom_request(uint8_t *key, int key_length, char* hash_func, int blm_idx){
    uint32_t md5_e_key = 0;
    uint32_t murmur_e_key = 0;
    uint32_t jhash_e_key = 5;
    int set_idx = 1;
    uint64_t cycle = 0;
    uint64_t start, end;
    //--------------------------------start
    if(strcmp(hash_func, "murmur3") == 0) {
        start = rdcycle();
        murmur_e_key = murmur3_hw(key, (size_t)key_length, 1000); // fixed seed: 1000
        end = rdcycle();
        set_idx = murmur_e_key % 8;
    }
    else if(strcmp(hash_func, "jhash") == 0) {
        start = rdcycle();
        jhash_e_key = jhash_hw(key, (size_t)key_length, INITVAL);
        end = rdcycle();
        set_idx = jhash_e_key % 8;
    }
    else {
        printf("No support for hash: %s\n", hash_func);
        printf("**PROCESS TERMINATED**\n");
        return -1;
    }
    //--------------------------------end
    cycle = end - start;
    bit_set(bloom_filter[blm_idx], set_idx);
    return cycle;
}

void result_analysis(uint64_t micro, uint64_t macro){
    uint64_t micro_result_int, macro_result_int;    // 정수부
    uint64_t micro_result_frac, macro_result_frac;  // 실수부

    printf("micro: %llu\n", micro);
    printf("macro: %llu\n", macro);

    micro_result_int = micro / WKSIZE;
    micro_result_frac = ((micro * 10000) / WKSIZE) % 10000;

    macro_result_int = macro / WKSIZE;
    macro_result_frac = ((macro * 10000) / WKSIZE) % 10000;

    printf("micro benckmark result: %llu.%llu\n", micro_result_int, micro_result_frac);
    printf("macro benckmark result: %llu.%llu\n", macro_result_int, macro_result_frac);
}

#define N_HASH  2

int main(void){
    printf("# PROGRAM START -----------------------------------------------------\n");
    
    int key_length = 36;
    uint8_t key[key_length];
	uint64_t micro_cycle;
    uint64_t micro_cycle_accum, macro_cycle_accum;
	uint64_t macro_start, macro_end;
    
	char *hash_list[N_HASH] = { "murmur3", "jhash" };
	//char *hash_list[2] = { "jhash", "murmur3" };
	char *hash_func;
    uint64_t micro_results[N_HASH] = { 0, };
    uint64_t macro_results[N_HASH] = { 0, };

	for (int hidx = 0; hidx < N_HASH; hidx++) {
		hash_func = hash_list[hidx];
		micro_cycle_accum = 0;
    	macro_cycle_accum = 0;

		printf("# HASH FUNCTION: %s\t --------------------------------\n", hash_func);
		bloom_init();
    
		for (int i = 0; i < WKSIZE; i++){
			int blm_idx = rand() % BLMNUM;
			key_generator(key, key_length);
			//printf("key:%s, length: %d\n", key, key_length); //erase this code if the program is completely worked!!
			macro_start = rdcycle();
			micro_cycle = bloom_request(key, key_length, hash_func, blm_idx);
			macro_end = rdcycle();
			uint64_t macro_cycle = macro_end - macro_start;

			micro_cycle_accum += micro_cycle;
			macro_cycle_accum += macro_cycle;
			printf("[%s_hw\t]: %llu\n", hash_func, micro_cycle);
			printf("[%s_hw+BLM\t]: %llu\n", hash_func, macro_cycle);
		}
        micro_results[hidx] = micro_cycle_accum;
        macro_results[hidx] = macro_cycle_accum;
	}
    printf("\n");

    for (int hidx = 0; hidx < N_HASH; hidx++) {
        hash_func = hash_list[hidx];
        printf("# RESULT of %s\t ------------------------------------\n", hash_func);
		result_analysis(micro_results[hidx], macro_results[hidx]);
		//printf("# ---------------------------------------------------------\n");
    }

    printf("# PROGRAM HALT ------------------------------------------------------\n");
    asm volatile ("ebreak");
    return 0;
}