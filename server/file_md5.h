#ifndef _FILE_MD5_H_
#define _FILE_MD5_H_


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct{
	uint64_t size;			// Size of input in bytes
	uint32_t buffer[4];		// Current accumulation of hash
	uint8_t input[64];		// Input to be used in the next step
	uint8_t digest[16];		// Result of algorithm
}MD5Context;


int md5File(const char *filename,char *md5);
void ByteToHexStr(const unsigned char* source, char* dest, int sourceLen);
void md5Init(MD5Context *ctx);
void md5Update(MD5Context *ctx, uint8_t *input, size_t input_len);
void md5Finalize(MD5Context *ctx);
void md5Step(uint32_t *buffer, uint32_t *input);

uint32_t F(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t G(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t H(uint32_t X, uint32_t Y, uint32_t Z);
uint32_t I(uint32_t X, uint32_t Y, uint32_t Z);

uint32_t rotate_left(uint32_t x, uint32_t n);

#endif