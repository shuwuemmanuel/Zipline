#include "sfx_builder.h"
#include "../sfx/sfx_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif

static void sfx_random_bytes(uint8_t *buf, size_t n)
{
#if defined(_WIN32)
    if (BCryptGenRandom(NULL, buf, (ULONG)n,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0)
        return;
    for (size_t i = 0; i < n; ++i)
        buf[i] = (uint8_t)rand();
#else
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(buf, 1, n, f);
        fclose(f);
        if (got == n)
            return;
    }
    for (size_t i = 0; i < n; ++i)
        buf[i] = (uint8_t)(rand() & 0xFF);
#endif
}

/* append u8/u16/u32/u64 little-endian to a growable buffer */
typedef struct { uint8_t *p; size_t len, cap; } Buf;

static int buf_reserve(Buf *b, size_t extra)
{
    if (b->len + extra <= b->cap)
        return 0;
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->len + extra)
        cap *= 2;
    uint8_t *np = (uint8_t *)realloc(b->p, cap);
    if (!np)
        return -1;
    b->p = np;
    b->cap = cap;
    return 0;
}
static int buf_bytes(Buf *b, const void *d, size_t n)
{
    if (buf_reserve(b, n)) return -1;
    memcpy(b->p + b->len, d, n);
    b->len += n;
    return 0;
}
static int buf_u8(Buf *b, uint8_t v)  { return buf_bytes(b, &v, 1); }
static int buf_u16(Buf *b, uint16_t v){ uint8_t t[2]={(uint8_t)v,(uint8_t)(v>>8)}; return buf_bytes(b,t,2); }
static int buf_u32(Buf *b, uint32_t v){ uint8_t t[4]={(uint8_t)v,(uint8_t)(v>>8),(uint8_t)(v>>16),(uint8_t)(v>>24)}; return buf_bytes(b,t,4); }
static int buf_u64(Buf *b, uint64_t v){ return buf_u32(b,(uint32_t)v) || buf_u32(b,(uint32_t)(v>>32)); }

static long file_size(FILE *f)
{
    if (fseek(f, 0, SEEK_END) != 0) return -1;
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    return s;
}

int sfx_build(const char *stub_path, const char *out_path,
              const SfxInput *inputs, int count,
              const char *settings, size_t settings_len,
              int level, const char *password,
              sfx_progress_fn progress, void *progress_ctx,
              char *err, size_t err_cap)
{
    int rc = -1;
    uint8_t *raw = NULL;
    uint8_t *comp = NULL;
    uint64_t *sizes = NULL;
    Buf payload = {0};
    FILE *out = NULL;
    FILE *stub = NULL;

#define FAIL(msg) do { if (err && err_cap) { strncpy(err, msg, err_cap - 1); err[err_cap - 1] = 0; } goto done; } while (0)

    int encrypt = (password && password[0]) ? 1 : 0;

    /* 1. read all input files into one solid buffer */
    sizes = (uint64_t *)calloc(count ? count : 1, sizeof(uint64_t));
    uint64_t raw_size = 0;
    for (int i = 0; i < count; ++i) {
        FILE *f = fopen(inputs[i].disk_path, "rb");
        if (!f) FAIL("Could not open an input file");
        long s = file_size(f);
        if (s < 0) { fclose(f); FAIL("Could not stat an input file"); }
        sizes[i] = (uint64_t)s;
        raw_size += (uint64_t)s;
        fclose(f);
    }
    raw = (uint8_t *)malloc(raw_size ? raw_size : 1);
    if (!raw) FAIL("Out of memory");
    {
        uint64_t off = 0;
        for (int i = 0; i < count; ++i) {
            FILE *f = fopen(inputs[i].disk_path, "rb");
            if (!f) FAIL("Could not re-open an input file");
            if (sizes[i]) {
                if (fread(raw + off, 1, (size_t)sizes[i], f) != (size_t)sizes[i]) {
                    fclose(f);
                    FAIL("Short read on input file");
                }
            }
            fclose(f);
            off += sizes[i];
        }
    }

    uint32_t raw_crc = sfx_crc32(raw, (size_t)raw_size);

    /* 2. compress */
    uint8_t method, prop = 0;
    size_t comp_len;
    if (level <= 0) {
        method = SFX_METHOD_STORE;
        comp = (uint8_t *)malloc(raw_size ? raw_size : 1);
        if (!comp) FAIL("Out of memory");
        memcpy(comp, raw, (size_t)raw_size);
        comp_len = (size_t)raw_size;
    } else {
        method = SFX_METHOD_LZMA2;
        if (sfx_lzma2_compress(raw, (size_t)raw_size, level, &prop,
                               &comp, &comp_len, progress, progress_ctx) != 0)
            FAIL("Compression failed");
    }

    /* 3. optional AES-256 encryption (pad to block size) */
    uint8_t salt[SFX_SALT_SIZE], iv[SFX_AES_BLOCK_SIZE];
    size_t stored_len = comp_len;
    memset(salt, 0, sizeof(salt));
    memset(iv, 0, sizeof(iv));
    if (encrypt) {
        sfx_random_bytes(salt, sizeof(salt));
        sfx_random_bytes(iv, sizeof(iv));
        stored_len = (comp_len + 15) & ~(size_t)15;
        uint8_t *padded = (uint8_t *)malloc(stored_len ? stored_len : 16);
        if (!padded) FAIL("Out of memory");
        memcpy(padded, comp, comp_len);
        if (stored_len > comp_len)
            memset(padded + comp_len, 0, stored_len - comp_len);
        free(comp);
        comp = padded;
        uint8_t key[SFX_AES_KEY_SIZE];
        sfx_derive_key(password, salt, key);
        sfx_aes_cbc_encrypt(key, iv, comp, stored_len);
        memset(key, 0, sizeof(key));
    }

    /* 4. build payload blob */
    if (buf_bytes(&payload, SFX_PAYLOAD_MAGIC, SFX_PAYLOAD_MAGIC_LEN)) FAIL("Out of memory");
    buf_u32(&payload, (uint32_t)settings_len);
    buf_bytes(&payload, settings, settings_len);
    buf_u8(&payload, method);
    buf_u8(&payload, (uint8_t)encrypt);
    buf_bytes(&payload, salt, SFX_SALT_SIZE);
    buf_bytes(&payload, iv, SFX_AES_BLOCK_SIZE);
    buf_u8(&payload, prop);
    buf_u64(&payload, raw_size);
    buf_u64(&payload, (uint64_t)comp_len);
    buf_u64(&payload, (uint64_t)stored_len);
    buf_u32(&payload, raw_crc);
    buf_u32(&payload, (uint32_t)count);
    for (int i = 0; i < count; ++i) {
        size_t nl = strlen(inputs[i].arc_name);
        if (nl > 0xFFFF) FAIL("Archive name too long");
        buf_u16(&payload, (uint16_t)nl);
        buf_bytes(&payload, inputs[i].arc_name, nl);
        buf_u64(&payload, sizes[i]);
        buf_u32(&payload, 0);
    }
    if (buf_bytes(&payload, comp, stored_len)) FAIL("Out of memory");

    /* 5. concatenate stub + payload + footer */
    stub = fopen(stub_path, "rb");
    if (!stub) FAIL("Could not open SFX stub");
    out = fopen(out_path, "wb");
    if (!out) FAIL("Could not create output file");

    uint64_t stub_size = 0;
    {
        uint8_t copybuf[65536];
        size_t n;
        while ((n = fread(copybuf, 1, sizeof(copybuf), stub)) > 0) {
            if (fwrite(copybuf, 1, n, out) != n) FAIL("Write error");
            stub_size += n;
        }
    }

    uint64_t payload_offset = stub_size;
    if (fwrite(payload.p, 1, payload.len, out) != payload.len) FAIL("Write error");

    SfxFooter footer;
    memcpy(footer.magic, SFX_FOOTER_MAGIC, SFX_FOOTER_MAGIC_LEN);
    footer.payload_offset = payload_offset;
    footer.payload_size = payload.len;
    if (fwrite(&footer, 1, SFX_FOOTER_SIZE, out) != SFX_FOOTER_SIZE) FAIL("Write error");

    rc = 0;

done:
    if (out) fclose(out);
    if (stub) fclose(stub);
    free(raw);
    free(comp);
    free(sizes);
    free(payload.p);
    return rc;
#undef FAIL
}
