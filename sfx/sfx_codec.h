/*
 * sfx_codec.h - Compression / encryption primitives shared by the Zipline
 * engine and the SFX stub.  Decode side is always available; encode side is
 * compiled only into the engine (where the LZMA2 encoder is linked in).
 */
#ifndef ZIPLINE_SFX_CODEC_H
#define ZIPLINE_SFX_CODEC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFX_AES_KEY_SIZE   32
#define SFX_AES_BLOCK_SIZE 16
#define SFX_SALT_SIZE      16
#define SFX_KDF_POWER      18 /* 2^18 = 262144 SHA-256 rounds */

/*
 * Derive a 256-bit AES key from a UTF-8 password using the 7-Zip style
 * iterated SHA-256 scheme (deterministic, identical on both sides).
 */
void sfx_derive_key(const char *password_utf8,
                    const uint8_t salt[SFX_SALT_SIZE],
                    uint8_t out_key[SFX_AES_KEY_SIZE]);

/*
 * Same 7-Zip style KDF but with an explicit cycle power and salt length, so
 * the 7z writer (power 19, 16-byte salt) and the SFX stub (power 18) share
 * one implementation.
 */
void sfx_derive_key_ex(const char *password_utf8,
                       const uint8_t *salt, size_t salt_len,
                       int cycle_power,
                       uint8_t out_key[SFX_AES_KEY_SIZE]);

/* CRC-32 (zlib polynomial) over a buffer. */
uint32_t sfx_crc32(const uint8_t *data, size_t len);

/*
 * AES-256-CBC in place.  'len' must be a multiple of 16.  Returns 0 on success.
 */
int sfx_aes_cbc_decrypt(const uint8_t key[SFX_AES_KEY_SIZE],
                        const uint8_t iv[SFX_AES_BLOCK_SIZE],
                        uint8_t *data, size_t len);

/*
 * Decompress an LZMA2 stream.  'prop' is the single LZMA2 properties byte.
 * Returns 0 on success; *out is malloc'd and must be freed by the caller.
 */
int sfx_lzma2_decompress(uint8_t prop,
                         const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len);

#ifndef SFX_DECODE_ONLY
/* Progress callback: fraction in [0,1]; return non-zero to abort. */
typedef int (*sfx_progress_fn)(void *ctx, double fraction);

int sfx_aes_cbc_encrypt(const uint8_t key[SFX_AES_KEY_SIZE],
                        const uint8_t iv[SFX_AES_BLOCK_SIZE],
                        uint8_t *data, size_t len);

/*
 * Compress 'in' with LZMA2 at the given level (0-9).  On success returns 0,
 * writes the properties byte to *prop, and sets *out (malloc'd) / *out_len.
 */
int sfx_lzma2_compress(const uint8_t *in, size_t in_len, int level,
                       uint8_t *prop, uint8_t **out, size_t *out_len,
                       sfx_progress_fn progress, void *progress_ctx);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZIPLINE_SFX_CODEC_H */
