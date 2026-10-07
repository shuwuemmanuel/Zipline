/* stub_util.h - small Win32 helpers shared by the stub translation units. */
#ifndef ZIPLINE_STUB_UTIL_H
#define ZIPLINE_STUB_UTIL_H

#ifdef _WIN32

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Convert a UTF-8 string to a freshly-allocated wide string (free with free()). */
static wchar_t *sfx_utf8_to_wide(const char *s)
{
    if (!s) s = "";
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) n = 1;
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0)
        w[0] = L'\0';
    return w;
}

/* Expand %VAR% references in a wide string (free with free()). */
static wchar_t *sfx_expand_env(const wchar_t *s)
{
    DWORD n = ExpandEnvironmentStringsW(s, NULL, 0);
    if (n == 0) {
        wchar_t *w = (wchar_t *)malloc((wcslen(s) + 1) * sizeof(wchar_t));
        if (w) wcscpy(w, s);
        return w;
    }
    wchar_t *w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w) return NULL;
    ExpandEnvironmentStringsW(s, w, n);
    return w;
}

/* Recursively create a directory (wide path), like `mkdir -p`. */
static void sfx_make_dirs(const wchar_t *path)
{
    size_t len = wcslen(path);
    wchar_t *tmp = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!tmp) return;
    wcscpy(tmp, path);
    for (size_t i = 0; i < len; ++i) {
        if (tmp[i] == L'/')
            tmp[i] = L'\\';
    }
    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == L'\\') {
            tmp[i] = L'\0';
            if (wcslen(tmp) > 0 && !(wcslen(tmp) == 2 && tmp[1] == L':'))
                CreateDirectoryW(tmp, NULL);
            tmp[i] = L'\\';
        }
    }
    CreateDirectoryW(tmp, NULL);
    free(tmp);
}

#endif /* _WIN32 */
#endif
