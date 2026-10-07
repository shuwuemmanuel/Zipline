/* stub_payload.h - parse the SFX payload blob from a memory buffer. */
#ifndef ZIPLINE_STUB_PAYLOAD_H
#define ZIPLINE_STUB_PAYLOAD_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char    *name;   /* UTF-8, '/'-separated, null-terminated (owned) */
    uint64_t size;
    uint32_t attr;
} SfxEntry;

typedef struct {
    char      *settings;      /* owned copy, modifiable, null-terminated */
    size_t     settings_len;
    uint8_t    method;
    uint8_t    encrypted;
    uint8_t    salt[16];
    uint8_t    iv[16];
    uint8_t    lzma_prop;
    uint64_t   raw_size;
    uint64_t   comp_size;
    uint64_t   stored_size;
    uint32_t   raw_crc;
    uint32_t   file_count;
    SfxEntry  *entries;       /* owned array */
    const uint8_t *data;      /* points into the payload buffer */
    size_t     data_len;
} SfxPayload;

/* Returns 0 on success.  On success call sfx_payload_free when done.
 * 'blob' must remain valid while 'data' is used. */
int  sfx_payload_parse(const uint8_t *blob, size_t blob_len, SfxPayload *p);
void sfx_payload_free(SfxPayload *p);

#endif
