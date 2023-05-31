//#include <stdio.h>
#include <uart.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "murmur.h"
#include "jhash.h"
#include "md5.h"

#define bit_set(v,n)	((v)[(n) >> 3] |= (0x1 << (0x7 - ((n) & 0x7))))
#define bit_get(v,n)	((v)[(n) >> 3] &  (0x1 << (0x7 - ((n) & 0x7))))
#define bit_clr(v,n)	((v)[(n) >> 3] &=~(0x1 << (0x7 - ((n) & 0x7))))

#define WKSIZE 10000
#define BLMNUM 1000

static unsigned char bloom_filter[BLMNUM][1];

static inline uint64_t rdcycle(void) {
    uint64_t n;
    asm volatile ( "rdcycle %0" : "=r"(n) );
    return n;
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
        murmur_e_key = murmur3_32(key, (size_t)key_length);
        end = rdcycle();
        set_idx = murmur_e_key % 8;
    }
    else if(strcmp(hash_func, "jhash") == 0) {
        start = rdcycle();
        jhash_e_key = jhash(key);
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

    // micro_result = micro/WKSIZE;
    // macro_result = macro/WKSIZE;
    printf("micro benckmark result: %llu.%llu\n", micro_result_int, micro_result_frac);
    printf("macro benckmark result: %llu.%llu\n", macro_result_int, macro_result_frac);
}

#define N_HASH 2

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
			printf("[%s_sw\t]: %llu\n", hash_func, micro_cycle);
			printf("[%s_sw+BLM\t]: %llu\n", hash_func, macro_cycle);
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