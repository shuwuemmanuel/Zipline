/* sevenzip_writer.h - minimal self-contained .7z writer.
 *
 * Produces a solid .7z archive from a set of files using LZMA2 compression and,
 * when a password is supplied, a second AES-256 coder (7-Zip's standard
 * 06F10701 method) so the result is fully compatible with 7-Zip / p7zr.
 *
 * This exists because libarchive can emit .7z but cannot encrypt it, and the
 * original Python tool used py7zr's AES-256 support. */
#ifndef ZIPLINE_SEVENZIP_WRITER_H
#define ZIPLINE_SEVENZIP_WRITER_H

#include <stddef.h>
#include "../sfx/sfx_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *disk_path; /* source file on disk                 */
    const char *arc_name;  /* name inside archive, '/'-separated   */
} SzInput;

/*
 * Write a .7z archive.  level 1-9 selects LZMA2 effort (<=0 => store via LZMA2
 * level 0).  password NULL/"" => no encryption.  Returns 0 on success.
 */
int sevenzip_write(const char *out_path,
                   const SzInput *inputs, int count,
                   int level, const char *password,
                   sfx_progress_fn progress, void *progress_ctx,
                   char *err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
