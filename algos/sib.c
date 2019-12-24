#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "../sha3/sph_blake.h"
#include "../sha3/sph_bmw.h"
#include "../sha3/sph_groestl.h"
#include "../sha3/sph_jh.h"
#include "../sha3/sph_keccak.h"
#include "../sha3/sph_skein.h"
#include "../sha3/sph_luffa.h"
#include "../sha3/sph_cubehash.h"
#include "../sha3/sph_shavite.h"
#include "../sha3/sph_simd.h"
#include "../sha3/sph_echo.h"

#include "gost.h"

void sib_hash(const char *input, char* output, uint32_t len)
{
	sph_blake512_context 	ctx_blake;
	sph_bmw512_context      ctx_bmw;
	sph_groestl512_context	ctx_groestl;
	sph_skein512_context	ctx_skein;
	sph_jh512_context       ctx_jh;
	sph_keccak512_context	ctx_keccak;
	sph_gost512_context 	ctx_gost;
	sph_luffa512_context 	ctx_luffa;
	sph_cubehash512_context ctx_cubehash;
	sph_shavite512_context  ctx_shavite;
	sph_simd512_context     ctx_simd;
	sph_echo512_context     ctx_echo;

	//these uint512 in the c++ source of the client are backed by an array of uint32
	uint32_t hashA[16], hashB[16];

	sph_blake512_init(&ctx_blake);
	sph_blake512(&ctx_blake, input, 80);
	sph_blake512_close(&ctx_blake, hashA);

	sph_bmw512_init(&ctx_bmw);
	sph_bmw512(&ctx_bmw, hashA, 64);
	sph_bmw512_close(&ctx_bmw, hashB);

	sph_groestl512_init(&ctx_groestl);
	sph_groestl512(&ctx_groestl, hashB, 64);
	sph_groestl512_close(&ctx_groestl, hashA);

	sph_skein512_init(&ctx_skein);
	sph_skein512(&ctx_skein, hashA, 64);
	sph_skein512_close(&ctx_skein, hashB);

	sph_jh512_init(&ctx_jh);
	sph_jh512(&ctx_jh, hashB, 64);
	sph_jh512_close(&ctx_jh, hashA);

	sph_keccak512_init(&ctx_keccak);
	sph_keccak512(&ctx_keccak, hashA, 64);
	sph_keccak512_close(&ctx_keccak, hashB);

	sph_gost512_init(&ctx_gost);
	sph_gost512(&ctx_gost, hashB, 64);
	sph_gost512_close(&ctx_gost, hashA);

	sph_luffa512_init(&ctx_luffa);
	sph_luffa512(&ctx_luffa, hashA, 64);
	sph_luffa512_close(&ctx_luffa, hashB);

	sph_cubehash512_init(&ctx_cubehash);
	sph_cubehash512(&ctx_cubehash, hashB, 64);
	sph_cubehash512_close(&ctx_cubehash, hashA);

	sph_shavite512_init(&ctx_shavite);
	sph_shavite512(&ctx_shavite, hashA, 64);
	sph_shavite512_close(&ctx_shavite, hashB);

	sph_simd512_init(&ctx_simd);
	sph_simd512(&ctx_simd, hashB, 64);
	sph_simd512_close(&ctx_simd, hashA);

	sph_echo512_init(&ctx_echo);
	sph_echo512(&ctx_echo, hashA, 64);
	sph_echo512_close(&ctx_echo, hashB);

	memcpy(output, hashB, 32);
}

