/* sfx_codec_enc.c - encode-side primitives (AES encrypt, LZMA2 compress).
 * Compiled only into the Zipline engine, never into the (decode-only) stub. */

#include "sfx_codec.h"

#include <stdlib.h>
#include <string.h>

#include "../third_party/lzma/Aes.h"
#include "../third_party/lzma/Lzma2Enc.h"
#include "../third_party/lzma/Alloc.h"

int sfx_aes_cbc_encrypt(const uint8_t key[SFX_AES_KEY_SIZE],
                        const uint8_t iv[SFX_AES_BLOCK_SIZE],
                        uint8_t *data, size_t len)
{
    if (len % SFX_AES_BLOCK_SIZE != 0)
        return -1;

    AesGenTables();

    UInt32 buf[AES_NUM_IVMRK_WORDS + 4];
    UInt32 *aes = buf;
    while (((uintptr_t)aes & 15) != 0)
        aes++;

    Aes_SetKey_Enc(aes + 4, key, SFX_AES_KEY_SIZE);
    AesCbc_Init(aes, iv);

    size_t num_blocks = len / SFX_AES_BLOCK_SIZE;
    if (((uintptr_t)data & 15) == 0) {
        AesCbc_Encode(aes, data, num_blocks);
    } else {
        uint8_t *tmp = (uint8_t *)malloc(len + 16);
        uint8_t *at = tmp;
        while (((uintptr_t)at & 15) != 0)
            at++;
        memcpy(at, data, len);
        AesCbc_Encode(aes, at, num_blocks);
        memcpy(data, at, len);
        free(tmp);
    }
    return 0;
}

/* progress adapter from the LZMA SDK ICompressProgress to sfx_progress_fn */
typedef struct {
    ICompressProgress vt;
    sfx_progress_fn fn;
    void *ctx;
    uint64_t total;
} ProgressWrap;

static SRes progress_cb(ICompressProgressPtr pp, UInt64 in_size, UInt64 out_size)
{
    ProgressWrap *w = Z7_CONTAINER_FROM_VTBL(pp, ProgressWrap, vt);
    (void)out_size;
    if (w->fn && w->total) {
        double frac = (double)in_size / (double)w->total;
        if (frac > 1.0) frac = 1.0;
        if (w->fn(w->ctx, frac))
            return SZ_ERROR_PROGRESS;
    }
    return SZ_OK;
}

int sfx_lzma2_compress(const uint8_t *in, size_t in_len, int level,
                       uint8_t *prop, uint8_t **out, size_t *out_len,
                       sfx_progress_fn progress, void *progress_ctx)
{
    CLzma2EncHandle enc = Lzma2Enc_Create(&g_Alloc, &g_BigAlloc);
    if (!enc)
        return -1;

    CLzma2EncProps props;
    Lzma2EncProps_Init(&props);
    if (level < 0) level = 0;
    if (level > 9) level = 9;
    props.lzmaProps.level = level;
    /* Keep the dictionary modest so the stub's decoder stays light. */
    props.lzmaProps.dictSize = (level <= 4) ? (1u << 20) : (1u << 24);
    Lzma2EncProps_Normalize(&props);
    if (Lzma2Enc_SetProps(enc, &props) != SZ_OK) {
        Lzma2Enc_Destroy(enc);
        return -2;
    }
    Lzma2Enc_SetDataSize(enc, in_len);

    *prop = Lzma2Enc_WriteProperties(enc);

    /* Worst-case output bound. */
    size_t cap = in_len + in_len / 2 + 4096;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (!buf) {
        Lzma2Enc_Destroy(enc);
        return -3;
    }

    ProgressWrap w;
    w.vt.Progress = progress_cb;
    w.fn = progress;
    w.ctx = progress_ctx;
    w.total = in_len ? in_len : 1;

    size_t dst = cap;
    SRes res = Lzma2Enc_Encode2(enc,
                                NULL, buf, &dst,
                                NULL, in, in_len,
                                (progress ? &w.vt : NULL));
    Lzma2Enc_Destroy(enc);

    if (res != SZ_OK) {
        free(buf);
        return -4;
    }
    *out = buf;
    *out_len = dst;
    return 0;
}
