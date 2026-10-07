#ifdef _WIN32

#include "stub_extract.h"
#include "stub_util.h"
#include "sfx_codec.h"
#include "sfx_format.h"

#include <stdlib.h>
#include <string.h>

static void set_err(wchar_t *err, size_t cap, const wchar_t *msg)
{
    if (err && cap) {
        wcsncpy(err, msg, cap - 1);
        err[cap - 1] = L'\0';
    }
}

int sfx_extract(const SfxPayload *p,
                const wchar_t *dest_dir,
                const char *password,
                int overwrite,
                SfxProgress prog, void *ctx,
                wchar_t *err, size_t err_cap)
{
    int rc = -1;
    uint8_t *work = NULL;
    uint8_t *raw = NULL;

    if (prog) prog(ctx, 10, "Decoding archive data...");

    /* Copy the stored (possibly encrypted) data so we can work in place. */
    work = (uint8_t *)malloc(p->data_len ? p->data_len : 1);
    if (!work) { set_err(err, err_cap, L"Out of memory"); goto done; }
    memcpy(work, p->data, p->data_len);

    if (p->encrypted) {
        if (prog) prog(ctx, 20, "Decrypting...");
        uint8_t key[SFX_AES_KEY_SIZE];
        sfx_derive_key(password ? password : "", p->salt, key);
        if (sfx_aes_cbc_decrypt(key, p->iv, work, p->data_len) != 0) {
            set_err(err, err_cap, L"Decryption failed");
            memset(key, 0, sizeof(key));
            goto done;
        }
        memset(key, 0, sizeof(key));
    }

    if (prog) prog(ctx, 40, "Decompressing...");

    raw = (uint8_t *)malloc(p->raw_size ? p->raw_size : 1);
    if (!raw) { set_err(err, err_cap, L"Out of memory"); goto done; }

    if (p->method == SFX_METHOD_STORE) {
        if (p->comp_size != p->raw_size) {
            set_err(err, err_cap, L"Corrupt archive");
            goto done;
        }
        memcpy(raw, work, (size_t)p->raw_size);
    } else {
        if (sfx_lzma2_decompress(p->lzma_prop, work, (size_t)p->comp_size,
                                 raw, (size_t)p->raw_size) != 0) {
            set_err(err, err_cap,
                    L"Decompression failed (wrong password or corrupt archive)");
            goto done;
        }
    }

    /* Integrity check. */
    if (sfx_crc32(raw, (size_t)p->raw_size) != p->raw_crc) {
        set_err(err, err_cap,
                L"Integrity check failed (wrong password or corrupt archive)");
        goto done;
    }

    if (prog) prog(ctx, 50, "Extracting files...");

    uint64_t offset = 0;
    for (uint32_t i = 0; i < p->file_count; ++i) {
        const SfxEntry *e = &p->entries[i];

        /* Build the full destination path. */
        wchar_t *rel = sfx_utf8_to_wide(e->name);
        if (!rel) { set_err(err, err_cap, L"Out of memory"); goto done; }
        for (wchar_t *q = rel; *q; ++q)
            if (*q == L'/') *q = L'\\';

        size_t full_len = wcslen(dest_dir) + 1 + wcslen(rel) + 1;
        wchar_t *full = (wchar_t *)malloc(full_len * sizeof(wchar_t));
        if (!full) { free(rel); set_err(err, err_cap, L"Out of memory"); goto done; }
        swprintf(full, full_len, L"%ls\\%ls", dest_dir, rel);
        free(rel);

        /* Ensure parent directory exists. */
        wchar_t *parent = (wchar_t *)malloc((wcslen(full) + 1) * sizeof(wchar_t));
        wcscpy(parent, full);
        wchar_t *slash = wcsrchr(parent, L'\\');
        if (slash) {
            *slash = L'\0';
            sfx_make_dirs(parent);
        }
        free(parent);

        int skip = 0;
        if (!overwrite) {
            DWORD attr = GetFileAttributesW(full);
            if (attr != INVALID_FILE_ATTRIBUTES)
                skip = 1;
        }

        if (!skip) {
            HANDLE h = CreateFileW(full, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h == INVALID_HANDLE_VALUE) {
                set_err(err, err_cap, L"Could not write a file to the destination");
                free(full);
                goto done;
            }
            uint64_t remaining = e->size;
            const uint8_t *src = raw + offset;
            while (remaining > 0) {
                DWORD chunk = remaining > (1u << 20) ? (1u << 20) : (DWORD)remaining;
                DWORD wrote = 0;
                if (!WriteFile(h, src, chunk, &wrote, NULL) || wrote != chunk) {
                    CloseHandle(h);
                    set_err(err, err_cap, L"Write error (disk full?)");
                    free(full);
                    goto done;
                }
                src += chunk;
                remaining -= chunk;
            }
            CloseHandle(h);
        }

        free(full);
        offset += e->size;

        if (prog) {
            int pct = 50 + (int)((i + 1) * 30 / (p->file_count ? p->file_count : 1));
            prog(ctx, pct, "Extracting files...");
        }
    }

    rc = 0;

done:
    if (work) { free(work); }
    if (raw) { free(raw); }
    return rc;
}

#endif /* _WIN32 */
