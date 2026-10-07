/* sfx_codec_dec.c - decode-side primitives (KDF, CRC, AES decrypt, LZMA2 decode). */

#include "sfx_codec.h"

#include <stdlib.h>
#include <string.h>

#include "../third_party/lzma/Sha256.h"
#include "../third_party/lzma/Aes.h"
#include "../third_party/lzma/Lzma2Dec.h"
#include "../third_party/lzma/Alloc.h"

/* ---- key derivation (7-Zip style iterated SHA-256) ---- */

void sfx_derive_key(const char *password_utf8,
                    const uint8_t salt[SFX_SALT_SIZE],
                    uint8_t out_key[SFX_AES_KEY_SIZE])
{
    sfx_derive_key_ex(password_utf8, salt, SFX_SALT_SIZE, SFX_KDF_POWER, out_key);
}

void sfx_derive_key_ex(const char *password_utf8,
                       const uint8_t *salt, size_t salt_len,
                       int cycle_power,
                       uint8_t out_key[SFX_AES_KEY_SIZE])
{
    /* Convert UTF-8 password to UTF-16LE (ASCII-safe path; higher code points
     * are passed through byte-wise, which is sufficient for matching both
     * ends since the engine encodes the same way). */
    size_t n = password_utf8 ? strlen(password_utf8) : 0;
    uint8_t *pw = (uint8_t *)malloc(n * 2 + 1);
    size_t pw_len = 0;
    for (size_t i = 0; i < n; ++i) {
        pw[pw_len++] = (uint8_t)password_utf8[i];
        pw[pw_len++] = 0;
    }

    CSha256 sha;
    Sha256_Init(&sha);

    uint64_t rounds = (uint64_t)1 << cycle_power;
    uint8_t ctr[8];
    memset(ctr, 0, sizeof(ctr));
    for (uint64_t r = 0; r < rounds; ++r) {
        if (salt_len)
            Sha256_Update(&sha, salt, salt_len);
        if (pw_len)
            Sha256_Update(&sha, pw, pw_len);
        Sha256_Update(&sha, ctr, 8);
        /* increment 64-bit little-endian counter */
        for (int i = 0; i < 8; ++i) {
            if (++ctr[i] != 0)
                break;
        }
    }
    Sha256_Final(&sha, out_key);

    if (pw) {
        memset(pw, 0, pw_len);
        free(pw);
    }
}

/* ---- CRC-32 ---- */

uint32_t sfx_crc32(const uint8_t *data, size_t len)
{
    static uint32_t table[256];
    static int built = 0;
    if (!built) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* ---- AES-256-CBC ---- */

static int aes_cbc(const uint8_t key[SFX_AES_KEY_SIZE],
                   const uint8_t iv[SFX_AES_BLOCK_SIZE],
                   uint8_t *data, size_t len, int encrypt);

int sfx_aes_cbc_decrypt(const uint8_t key[SFX_AES_KEY_SIZE],
                        const uint8_t iv[SFX_AES_BLOCK_SIZE],
                        uint8_t *data, size_t len)
{
    return aes_cbc(key, iv, data, len, 0);
}

static int aes_cbc(const uint8_t key[SFX_AES_KEY_SIZE],
                   const uint8_t iv[SFX_AES_BLOCK_SIZE],
                   uint8_t *data, size_t len, int encrypt)
{
    if (len % SFX_AES_BLOCK_SIZE != 0)
        return -1;

    AesGenTables();

    /* 16-byte aligned IV+keyMode+roundKeys area. */
    UInt32 buf[AES_NUM_IVMRK_WORDS + 4];
    UInt32 *aes = buf;
    while (((uintptr_t)aes & 15) != 0)
        aes++;

    if (encrypt)
        Aes_SetKey_Enc(aes + 4, key, SFX_AES_KEY_SIZE);
    else
        Aes_SetKey_Dec(aes + 4, key, SFX_AES_KEY_SIZE);
    AesCbc_Init(aes, iv);

    size_t num_blocks = len / SFX_AES_BLOCK_SIZE;

    /* The AES code requires a 16-byte aligned data pointer; copy through an
     * aligned bounce buffer when necessary. */
    if (((uintptr_t)data & 15) == 0) {
#ifdef SFX_DECODE_ONLY
        AesCbc_Decode(aes, data, num_blocks);
#else
        if (encrypt)
            AesCbc_Encode(aes, data, num_blocks);
        else
            AesCbc_Decode(aes, data, num_blocks);
#endif
    } else {
        uint8_t *tmp = (uint8_t *)malloc(len + 16);
        uint8_t *at = tmp;
        while (((uintptr_t)at & 15) != 0)
            at++;
        memcpy(at, data, len);
#ifdef SFX_DECODE_ONLY
        AesCbc_Decode(aes, at, num_blocks);
#else
        if (encrypt)
            AesCbc_Encode(aes, at, num_blocks);
        else
            AesCbc_Decode(aes, at, num_blocks);
#endif
        memcpy(data, at, len);
        free(tmp);
    }
    return 0;
}

/* ---- LZMA2 decompress ---- */

int sfx_lzma2_decompress(uint8_t prop,
                         const uint8_t *in, size_t in_len,
                         uint8_t *out, size_t out_len)
{
    CLzma2Dec dec;
    Lzma2Dec_Construct(&dec);
    if (Lzma2Dec_Allocate(&dec, prop, &g_Alloc) != SZ_OK)
        return -1;
    Lzma2Dec_Init(&dec);

    SizeT dst_len = out_len;
    SizeT src_len = in_len;
    ELzmaStatus status;
    SRes res = Lzma2Dec_DecodeToBuf(&dec, out, &dst_len,
                                    in, &src_len,
                                    LZMA_FINISH_END, &status);
    Lzma2Dec_Free(&dec, &g_Alloc);

    if (res != SZ_OK)
        return -2;
    if (dst_len != out_len)
        return -3;
    return 0;
}
