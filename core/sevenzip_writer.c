#include "sevenzip_writer.h"
#include "../sfx/sfx_format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- growable byte buffer ---- */
typedef struct { uint8_t *p; size_t len, cap; } Buf;
static int buf_need(Buf *b, size_t extra)
{
    if (b->len + extra <= b->cap) return 0;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < b->len + extra) cap *= 2;
    uint8_t *np = (uint8_t *)realloc(b->p, cap);
    if (!np) return -1;
    b->p = np; b->cap = cap; return 0;
}
static void buf_u8(Buf *b, uint8_t v) { if (!buf_need(b, 1)) b->p[b->len++] = v; }
static void buf_mem(Buf *b, const void *d, size_t n) { if (!buf_need(b, n)) { memcpy(b->p + b->len, d, n); b->len += n; } }

/* 7z variable-length number encoding. */
static void buf_num(Buf *b, uint64_t value)
{
    uint8_t first = 0, mask = 0x80;
    int i;
    for (i = 0; i < 8; i++) {
        if (value < ((uint64_t)1 << (7 * (i + 1)))) {
            first |= (uint8_t)(value >> (8 * i));
            break;
        }
        first |= mask;
        mask >>= 1;
    }
    buf_u8(b, first);
    for (; i > 0; i--) {
        buf_u8(b, (uint8_t)value);
        value >>= 8;
    }
}

/* 7z property IDs */
enum {
    k_End = 0, k_Header = 1, k_MainStreamsInfo = 4, k_FilesInfo = 5,
    k_PackInfo = 6, k_UnpackInfo = 7, k_SubStreamsInfo = 8, k_Size = 9,
    k_CRC = 10, k_Folder = 11, k_CodersUnpackSize = 12, k_NumUnpackStream = 13,
    k_Name = 17, k_EncodedHeader = 23
};

static const uint8_t k_sig[6] = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };

static uint32_t crc32_calc(const uint8_t *d, size_t n) { return sfx_crc32(d, n); }

static long fsize(FILE *f)
{
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    return s;
}

int sevenzip_write(const char *out_path,
                   const SzInput *inputs, int count,
                   int level, const char *password,
                   sfx_progress_fn progress, void *progress_ctx,
                   char *err, size_t err_cap)
{
    int rc = -1;
    uint8_t *raw = NULL, *comp = NULL, *stored = NULL;
    uint64_t *sizes = NULL;
    uint32_t *crcs = NULL;
    Buf header = {0};
    FILE *out = NULL;

#define FAIL(m) do { if (err && err_cap) { strncpy(err, m, err_cap - 1); err[err_cap - 1] = 0; } goto done; } while (0)

    int encrypt = (password && password[0]) ? 1 : 0;

    /* read files into one solid buffer, recording sizes and CRCs */
    sizes = (uint64_t *)calloc(count ? count : 1, sizeof(uint64_t));
    crcs = (uint32_t *)calloc(count ? count : 1, sizeof(uint32_t));
    uint64_t raw_size = 0;
    for (int i = 0; i < count; ++i) {
        FILE *f = fopen(inputs[i].disk_path, "rb");
        if (!f) FAIL("Could not open input file");
        long s = fsize(f);
        fclose(f);
        if (s < 0) FAIL("Could not stat input file");
        sizes[i] = (uint64_t)s;
        raw_size += (uint64_t)s;
    }
    raw = (uint8_t *)malloc(raw_size ? raw_size : 1);
    if (!raw) FAIL("Out of memory");
    {
        uint64_t off = 0;
        for (int i = 0; i < count; ++i) {
            FILE *f = fopen(inputs[i].disk_path, "rb");
            if (!f) FAIL("Could not re-open input file");
            if (sizes[i] && fread(raw + off, 1, (size_t)sizes[i], f) != (size_t)sizes[i]) {
                fclose(f);
                FAIL("Short read");
            }
            fclose(f);
            crcs[i] = crc32_calc(raw + off, (size_t)sizes[i]);
            off += sizes[i];
        }
    }

    /* compress with LZMA2 */
    uint8_t prop = 0;
    size_t comp_len = 0;
    int lvl = level <= 0 ? 0 : level;
    if (sfx_lzma2_compress(raw, (size_t)raw_size, lvl, &prop,
                           &comp, &comp_len, progress, progress_ctx) != 0)
        FAIL("Compression failed");

    /* optional AES-256 layer */
    uint8_t salt[16], iv[16];
    size_t pack_len = comp_len;
    stored = comp;
    if (encrypt) {
        /* random salt + iv */
        FILE *ur = fopen("/dev/urandom", "rb");
        if (ur) { if (fread(salt, 1, 16, ur) != 16 || fread(iv, 1, 16, ur) != 16) {} fclose(ur); }
        else { for (int i = 0; i < 16; i++) { salt[i] = (uint8_t)rand(); iv[i] = (uint8_t)rand(); } }

        pack_len = (comp_len + 15) & ~(size_t)15;
        stored = (uint8_t *)malloc(pack_len ? pack_len : 16);
        if (!stored) FAIL("Out of memory");
        memcpy(stored, comp, comp_len);
        if (pack_len > comp_len) memset(stored + comp_len, 0, pack_len - comp_len);

        uint8_t key[32];
        sfx_derive_key_ex(password, salt, 16, 19, key);
        sfx_aes_cbc_encrypt(key, iv, stored, pack_len);
        memset(key, 0, sizeof(key));
    }

    /* ---- build the end header ---- */
    Buf *h = &header;
    buf_u8(h, k_Header);

    buf_u8(h, k_MainStreamsInfo);

    /* PackInfo */
    buf_u8(h, k_PackInfo);
    buf_num(h, 0);           /* pack position */
    buf_num(h, 1);           /* number of pack streams */
    buf_u8(h, k_Size);
    buf_num(h, pack_len);
    buf_u8(h, k_End);

    /* UnpackInfo */
    buf_u8(h, k_UnpackInfo);
    buf_u8(h, k_Folder);
    buf_num(h, 1);           /* one folder */
    buf_u8(h, 0);            /* external = 0 (folder defined here) */

    if (!encrypt) {
        buf_num(h, 1);       /* numCoders */
        buf_u8(h, 0x21);     /* idSize=1, hasAttributes */
        buf_u8(h, 0x21);     /* LZMA2 method id */
        buf_num(h, 1);       /* prop size */
        buf_u8(h, prop);
        /* 1 coder => 1 out stream, no bind pairs, 1 pack stream (implicit) */
        buf_u8(h, k_CodersUnpackSize);
        buf_num(h, raw_size);
    } else {
        buf_num(h, 2);       /* numCoders: LZMA2 + AES */
        /* coder 0: LZMA2 */
        buf_u8(h, 0x21);
        buf_u8(h, 0x21);
        buf_num(h, 1);
        buf_u8(h, prop);
        /* coder 1: AES-256 (06 F1 07 01) */
        buf_u8(h, 0x24);     /* idSize=4, hasAttributes */
        buf_u8(h, 0x06); buf_u8(h, 0xF1); buf_u8(h, 0x07); buf_u8(h, 0x01);
        {
            uint8_t props[2 + 16 + 16];
            props[0] = (uint8_t)(19 | 0x80 | 0x40); /* numCyclesPower=19, salt+iv present */
            props[1] = (uint8_t)((15 << 4) | 15);   /* saltSize=1+15=16, ivSize=1+15=16 */
            memcpy(props + 2, salt, 16);
            memcpy(props + 18, iv, 16);
            buf_num(h, sizeof(props));
            buf_mem(h, props, sizeof(props));
        }
        /* bind pair: in-stream 0 (LZMA2 input) <= out-stream 1 (AES output) */
        buf_num(h, 0);       /* InIndex  */
        buf_num(h, 1);       /* OutIndex */
        /* numPackStreams == 1 (implicit: the unbound in-stream, AES input) */
        buf_u8(h, k_CodersUnpackSize);
        buf_num(h, raw_size);   /* out-stream 0: LZMA2 output (plaintext)       */
        buf_num(h, pack_len);   /* out-stream 1: AES output (block-aligned len) */
    }
    buf_u8(h, k_End);        /* end UnpackInfo */

    /* SubStreamsInfo: n substreams in the single folder */
    buf_u8(h, k_SubStreamsInfo);
    buf_u8(h, k_NumUnpackStream);
    buf_num(h, (uint64_t)count);
    if (count > 1) {
        buf_u8(h, k_Size);
        for (int i = 0; i < count - 1; ++i)  /* last size is implicit */
            buf_num(h, sizes[i]);
    }
    buf_u8(h, k_CRC);
    buf_u8(h, 1);            /* all defined */
    for (int i = 0; i < count; ++i) {
        uint8_t le[4] = { (uint8_t)crcs[i], (uint8_t)(crcs[i] >> 8),
                          (uint8_t)(crcs[i] >> 16), (uint8_t)(crcs[i] >> 24) };
        buf_mem(h, le, 4);
    }
    buf_u8(h, k_End);        /* end SubStreamsInfo */

    buf_u8(h, k_End);        /* end MainStreamsInfo */

    /* FilesInfo */
    buf_u8(h, k_FilesInfo);
    buf_num(h, (uint64_t)count);
    /* names property */
    {
        Buf names = {0};
        buf_u8(&names, 0);   /* external = 0 */
        for (int i = 0; i < count; ++i) {
            const char *s = inputs[i].arc_name;
            for (size_t j = 0; s[j]; ++j) {
                uint8_t ch = (uint8_t)s[j];
                uint16_t w = (s[j] == '/') ? (uint16_t)'\\' : ch; /* 7z uses '\\' */
                buf_u8(&names, (uint8_t)w);
                buf_u8(&names, (uint8_t)(w >> 8));
            }
            buf_u8(&names, 0); buf_u8(&names, 0); /* UTF-16 NUL */
        }
        buf_u8(h, k_Name);
        buf_num(h, names.len);
        buf_mem(h, names.p, names.len);
        free(names.p);
    }
    buf_u8(h, k_End);        /* end FilesInfo */

    buf_u8(h, k_End);        /* end Header */

    /* ---- write the file ---- */
    out = fopen(out_path, "wb");
    if (!out) FAIL("Could not create output file");

    uint8_t start_header[32];
    memset(start_header, 0, sizeof(start_header));
    memcpy(start_header, k_sig, 6);
    start_header[6] = 0;  /* version major */
    start_header[7] = 4;  /* version minor */

    uint64_t next_offset = pack_len;         /* header sits right after packed data */
    uint64_t next_size = header.len;
    uint32_t next_crc = crc32_calc(header.p, header.len);

    uint8_t *sh = start_header + 12;
    for (int i = 0; i < 8; i++) sh[i] = (uint8_t)(next_offset >> (8 * i));
    for (int i = 0; i < 8; i++) sh[8 + i] = (uint8_t)(next_size >> (8 * i));
    sh[16] = (uint8_t)next_crc; sh[17] = (uint8_t)(next_crc >> 8);
    sh[18] = (uint8_t)(next_crc >> 16); sh[19] = (uint8_t)(next_crc >> 24);

    uint32_t start_crc = crc32_calc(start_header + 12, 20);
    start_header[8] = (uint8_t)start_crc; start_header[9] = (uint8_t)(start_crc >> 8);
    start_header[10] = (uint8_t)(start_crc >> 16); start_header[11] = (uint8_t)(start_crc >> 24);

    if (fwrite(start_header, 1, 32, out) != 32) FAIL("Write error");
    if (pack_len && fwrite(stored, 1, pack_len, out) != pack_len) FAIL("Write error");
    if (header.len && fwrite(header.p, 1, header.len, out) != header.len) FAIL("Write error");

    rc = 0;

done:
    if (out) fclose(out);
    free(raw);
    if (stored && stored != comp) free(stored);
    free(comp);
    free(sizes);
    free(crcs);
    free(header.p);
    return rc;
#undef FAIL
}
