#ifndef EQUI_H
#define EQUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sodium.h>

void equi_hash(const char* input, char* output, uint32_t len);

void digestInit(crypto_generichash_blake2b_state *S, const int n, const int k);

static void expandArray(const unsigned char *in, const size_t in_len,
    unsigned char *out, const size_t out_len,
    const size_t bit_len, const size_t byte_pad);

static int isZero(const uint8_t *hash, size_t len);

// hdr -> header including nonce (140 bytes)
// soln -> equihash solution (excluding 3 bytes with size, so 1344 bytes length)

bool verifyEH(const char *hdr, const char *soln);

#ifdef __cplusplus
}
#endif

#endif

