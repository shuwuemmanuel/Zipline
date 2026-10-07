#include "stub_payload.h"
#include "sfx_format.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *p;
    const uint8_t *end;
    int error;
} Cursor;

static uint8_t rd_u8(Cursor *c)
{
    if (c->p + 1 > c->end) { c->error = 1; return 0; }
    return *c->p++;
}
static uint16_t rd_u16(Cursor *c)
{
    if (c->p + 2 > c->end) { c->error = 1; return 0; }
    uint16_t v = (uint16_t)(c->p[0] | (c->p[1] << 8));
    c->p += 2;
    return v;
}
static uint32_t rd_u32(Cursor *c)
{
    if (c->p + 4 > c->end) { c->error = 1; return 0; }
    uint32_t v = (uint32_t)c->p[0] | ((uint32_t)c->p[1] << 8) |
                 ((uint32_t)c->p[2] << 16) | ((uint32_t)c->p[3] << 24);
    c->p += 4;
    return v;
}
static uint64_t rd_u64(Cursor *c)
{
    uint64_t lo = rd_u32(c);
    uint64_t hi = rd_u32(c);
    return lo | (hi << 32);
}
static void rd_bytes(Cursor *c, void *dst, size_t n)
{
    if (c->p + n > c->end) { c->error = 1; return; }
    memcpy(dst, c->p, n);
    c->p += n;
}

int sfx_payload_parse(const uint8_t *blob, size_t blob_len, SfxPayload *p)
{
    memset(p, 0, sizeof(*p));

    Cursor c;
    c.p = blob;
    c.end = blob + blob_len;
    c.error = 0;

    char magic[SFX_PAYLOAD_MAGIC_LEN];
    rd_bytes(&c, magic, SFX_PAYLOAD_MAGIC_LEN);
    if (c.error || memcmp(magic, SFX_PAYLOAD_MAGIC, SFX_PAYLOAD_MAGIC_LEN) != 0)
        return -1;

    uint32_t settings_len = rd_u32(&c);
    p->settings = (char *)malloc(settings_len + 1);
    if (!p->settings) return -2;
    rd_bytes(&c, p->settings, settings_len);
    p->settings[settings_len] = '\0';
    p->settings_len = settings_len;

    p->method = rd_u8(&c);
    p->encrypted = rd_u8(&c);
    rd_bytes(&c, p->salt, 16);
    rd_bytes(&c, p->iv, 16);
    p->lzma_prop = rd_u8(&c);
    p->raw_size = rd_u64(&c);
    p->comp_size = rd_u64(&c);
    p->stored_size = rd_u64(&c);
    p->raw_crc = rd_u32(&c);
    p->file_count = rd_u32(&c);

    if (c.error)
        goto fail;

    if (p->file_count > 1000000u)
        goto fail;

    p->entries = (SfxEntry *)calloc(p->file_count ? p->file_count : 1,
                                    sizeof(SfxEntry));
    if (!p->entries)
        goto fail;

    for (uint32_t i = 0; i < p->file_count; ++i) {
        uint16_t nl = rd_u16(&c);
        if (c.error) goto fail;
        char *name = (char *)malloc(nl + 1);
        if (!name) goto fail;
        rd_bytes(&c, name, nl);
        name[nl] = '\0';
        p->entries[i].name = name;
        p->entries[i].size = rd_u64(&c);
        p->entries[i].attr = rd_u32(&c);
        if (c.error) goto fail;
    }

    /* remaining bytes are the stored data region */
    if ((uint64_t)(c.end - c.p) < p->stored_size)
        goto fail;
    p->data = c.p;
    p->data_len = (size_t)p->stored_size;
    return 0;

fail:
    sfx_payload_free(p);
    return -3;
}

void sfx_payload_free(SfxPayload *p)
{
    if (p->settings) { free(p->settings); p->settings = NULL; }
    if (p->entries) {
        for (uint32_t i = 0; i < p->file_count; ++i)
            free(p->entries[i].name);
        free(p->entries);
        p->entries = NULL;
    }
    p->file_count = 0;
}
