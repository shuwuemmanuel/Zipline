/* stub_extract.h - decode + write the payload's files to disk (Win32). */
#ifndef ZIPLINE_STUB_EXTRACT_H
#define ZIPLINE_STUB_EXTRACT_H

#ifdef _WIN32
#include <windows.h>
#include "stub_payload.h"

/* Progress callback: percentage 0-100 and a status string (UTF-8). */
typedef void (*SfxProgress)(void *ctx, int percent, const char *status);

/*
 * Decrypt/decompress the payload and write every file under dest_dir.
 * 'password' may be NULL when the payload is not encrypted.
 * Returns 0 on success; on failure writes a UTF-16 message into err.
 */
int sfx_extract(const SfxPayload *p,
                const wchar_t *dest_dir,
                const char *password,
                int overwrite,
                SfxProgress prog, void *ctx,
                wchar_t *err, size_t err_cap);

#endif
#endif
