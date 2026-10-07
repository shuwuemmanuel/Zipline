/* sfx_builder.h - assemble a Zipline self-extracting executable.
 *
 * Produces  [stub.exe][payload][footer]  from a list of input files plus a
 * settings blob.  Used by the Zipline engine's EXE format and reused by the
 * test harness.  Pure C so it links against the LZMA encoder directly. */
#ifndef ZIPLINE_SFX_BUILDER_H
#define ZIPLINE_SFX_BUILDER_H

#include <stddef.h>
#include <stdint.h>
#include "../sfx/sfx_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *disk_path; /* source file on disk                       */
    const char *arc_name;  /* name inside the archive, '/'-separated     */
} SfxInput;

/*
 * Build a self-extracting executable.
 *   stub_path    : path to the prebuilt stub.exe
 *   out_path     : destination .exe
 *   inputs/count : files to embed
 *   settings     : "key\tvalue\n" blob (newlines in values escaped as \n)
 *   level        : 0 => store, 1-9 => LZMA2 level
 *   password     : NULL/"" => no encryption, otherwise AES-256
 * Returns 0 on success; on failure fills 'err'.
 */
int sfx_build(const char *stub_path, const char *out_path,
              const SfxInput *inputs, int count,
              const char *settings, size_t settings_len,
              int level, const char *password,
              sfx_progress_fn progress, void *progress_ctx,
              char *err, size_t err_cap);

#ifdef __cplusplus
}
#endif

#endif
