#include "stub_settings.h"

#include <stdlib.h>
#include <string.h>

static char *unescape_newlines(const char *v)
{
    size_t n = strlen(v);
    char *out = (char *)malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; ++i) {
        if (v[i] == '\\' && i + 1 < n && v[i + 1] == 'n') {
            out[j++] = '\n';
            ++i;
        } else {
            out[j++] = v[i];
        }
    }
    out[j] = '\0';
    return out;
}

void sfx_settings_parse(SfxSettings *s, char *buf, size_t len)
{
    s->count = 0;
    size_t i = 0;
    while (i < len && s->count < SFX_MAX_SETTINGS) {
        size_t start = i;
        while (i < len && buf[i] != '\n')
            ++i;
        /* line is buf[start..i) */
        size_t line_end = i;
        if (i < len)
            ++i; /* skip newline */
        if (line_end == start)
            continue;
        /* split on first tab */
        size_t t = start;
        while (t < line_end && buf[t] != '\t')
            ++t;
        if (t >= line_end)
            continue;
        buf[t] = '\0';
        char key_terminated_val[1];
        (void)key_terminated_val;
        /* value substring */
        char saved = buf[line_end];
        buf[line_end] = '\0';
        const char *raw_val = &buf[t + 1];
        s->keys[s->count] = &buf[start];
        s->vals[s->count] = unescape_newlines(raw_val);
        buf[line_end] = saved;
        s->count++;
    }
}

void sfx_settings_free(SfxSettings *s)
{
    for (int i = 0; i < s->count; ++i)
        free(s->vals[i]);
    s->count = 0;
}

const char *sfx_settings_get(const SfxSettings *s, const char *key, const char *def)
{
    for (int i = 0; i < s->count; ++i) {
        if (strcmp(s->keys[i], key) == 0)
            return s->vals[i];
    }
    return def;
}

int sfx_settings_get_bool(const SfxSettings *s, const char *key, int def)
{
    const char *v = sfx_settings_get(s, key, NULL);
    if (!v)
        return def;
    if (v[0] == '1' || v[0] == 't' || v[0] == 'T' || v[0] == 'y' || v[0] == 'Y')
        return 1;
    if (v[0] == '0' || v[0] == 'f' || v[0] == 'F' || v[0] == 'n' || v[0] == 'N')
        return 0;
    return def;
}
