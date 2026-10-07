/* stub_settings.h - tiny key/value store for the payload's settings blob. */
#ifndef ZIPLINE_STUB_SETTINGS_H
#define ZIPLINE_STUB_SETTINGS_H

#include <stddef.h>

#define SFX_MAX_SETTINGS 64

typedef struct {
    char *keys[SFX_MAX_SETTINGS];
    char *vals[SFX_MAX_SETTINGS];
    int   count;
} SfxSettings;

/*
 * Parse a settings blob of "key\tvalue\n" lines.  Within a value the two
 * characters '\' 'n' are unescaped to a real newline.  The blob is modified
 * in place and the struct points into 'buf' (plus small allocations for
 * unescaped values); call sfx_settings_free when done.
 */
void sfx_settings_parse(SfxSettings *s, char *buf, size_t len);
void sfx_settings_free(SfxSettings *s);

/* Returns the value for 'key' or 'def' (which may be NULL) if absent. */
const char *sfx_settings_get(const SfxSettings *s, const char *key, const char *def);

/* Boolean lookup: "1"/"true"/"yes" (case-insensitive) => 1, else 'def'. */
int sfx_settings_get_bool(const SfxSettings *s, const char *key, int def);

#endif
