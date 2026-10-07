/*
 * stub_main.c - Zipline self-extracting archive runtime (Windows).
 *
 * This is the native replacement for the old PyInstaller + tkinter extractor.
 * One generic stub.exe is produced at build time; the Zipline engine appends a
 * payload + footer to a copy of it to mint each self-extracting archive.  All
 * per-archive behaviour (title, description, extraction defaults, shortcuts,
 * PATH / registry integration, run-after, silent mode, ...) is driven by the
 * settings blob embedded in the payload, so the stub never needs recompiling.
 */

#ifndef _WIN32
#error "stub_main.c targets Windows"
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "sfx_format.h"
#include "stub_payload.h"
#include "stub_settings.h"
#include "stub_extract.h"
#include "stub_util.h"

#include "stub_resource.h"

/* ---- globals shared across the UI ---- */
static uint8_t    *g_blob = NULL;     /* raw payload bytes (kept alive) */
static SfxPayload  g_payload;
static SfxSettings g_settings;

static HINSTANCE g_hinst;
static HWND  g_hwnd;
static HWND  g_path_edit, g_pass_edit, g_progress, g_status, g_extract_btn, g_cancel_btn;
static HFONT g_font, g_font_bold, g_font_title;
static HICON g_app_icon;
static int   g_has_password_ui = 0;
static volatile LONG g_running = 0;

#define WM_APP_PROGRESS (WM_APP + 1)
#define WM_APP_DONE     (WM_APP + 2)

/* ------------------------------------------------------------------ */
/* Reading the payload out of our own executable                       */
/* ------------------------------------------------------------------ */

static int read_self_payload(void)
{
    wchar_t self[MAX_PATH];
    if (!GetModuleFileNameW(NULL, self, MAX_PATH))
        return -1;

    HANDLE h = CreateFileW(self, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < SFX_FOOTER_SIZE) {
        CloseHandle(h);
        return -1;
    }

    /* read footer */
    SfxFooter footer;
    LARGE_INTEGER pos;
    pos.QuadPart = size.QuadPart - SFX_FOOTER_SIZE;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);
    DWORD got = 0;
    if (!ReadFile(h, &footer, SFX_FOOTER_SIZE, &got, NULL) || got != SFX_FOOTER_SIZE) {
        CloseHandle(h);
        return -1;
    }
    if (memcmp(footer.magic, SFX_FOOTER_MAGIC, SFX_FOOTER_MAGIC_LEN) != 0) {
        CloseHandle(h);
        return -2; /* no payload attached */
    }
    if (footer.payload_offset + footer.payload_size > (uint64_t)size.QuadPart) {
        CloseHandle(h);
        return -1;
    }

    g_blob = (uint8_t *)malloc((size_t)footer.payload_size);
    if (!g_blob) { CloseHandle(h); return -1; }

    pos.QuadPart = (LONGLONG)footer.payload_offset;
    SetFilePointerEx(h, pos, NULL, FILE_BEGIN);

    uint64_t remaining = footer.payload_size;
    uint8_t *dst = g_blob;
    while (remaining > 0) {
        DWORD chunk = remaining > (1u << 20) ? (1u << 20) : (DWORD)remaining;
        DWORD rd = 0;
        if (!ReadFile(h, dst, chunk, &rd, NULL) || rd == 0) {
            CloseHandle(h);
            return -1;
        }
        dst += rd;
        remaining -= rd;
    }
    CloseHandle(h);

    if (sfx_payload_parse(g_blob, (size_t)footer.payload_size, &g_payload) != 0)
        return -3;

    sfx_settings_parse(&g_settings, g_payload.settings, g_payload.settings_len);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Destination path resolution                                         */
/* ------------------------------------------------------------------ */

static void archive_base_name(wchar_t *out, size_t cap)
{
    const char *an = sfx_settings_get(&g_settings, "archive_name", NULL);
    if (an && an[0]) {
        wchar_t *w = sfx_utf8_to_wide(an);
        wcsncpy(out, w, cap - 1);
        out[cap - 1] = L'\0';
        free(w);
        return;
    }
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    wchar_t *base = wcsrchr(self, L'\\');
    base = base ? base + 1 : self;
    wchar_t *dot = wcsrchr(base, L'.');
    if (dot) *dot = L'\0';
    wcsncpy(out, base, cap - 1);
    out[cap - 1] = L'\0';
}

static void resolve_default_path(wchar_t *out, size_t cap)
{
    const char *dp = sfx_settings_get(&g_settings, "default_extract_path",
                                      "%USERPROFILE%\\Desktop");
    if (strcmp(dp, "Same folder as executable") == 0) {
        wchar_t self[MAX_PATH];
        GetModuleFileNameW(NULL, self, MAX_PATH);
        wchar_t *slash = wcsrchr(self, L'\\');
        if (slash) *slash = L'\0';
        wcsncpy(out, self, cap - 1);
        out[cap - 1] = L'\0';
        return;
    }
    if (strcmp(dp, "Ask user (show browse dialog)") == 0) {
        wchar_t home[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, home))) {
            wcsncpy(out, home, cap - 1);
            out[cap - 1] = L'\0';
        }
        return;
    }
    wchar_t *w = sfx_utf8_to_wide(dp);
    wchar_t *ex = sfx_expand_env(w);
    wcsncpy(out, ex, cap - 1);
    out[cap - 1] = L'\0';
    free(w);
    free(ex);
}

/* Apply create_subfolder: append the archive base name if requested. */
static void apply_subfolder(wchar_t *path, size_t cap)
{
    if (sfx_settings_get_bool(&g_settings, "create_subfolder", 1)) {
        wchar_t base[MAX_PATH];
        archive_base_name(base, MAX_PATH);
        size_t l = wcslen(path);
        if (l && path[l - 1] != L'\\' && l + 1 < cap) {
            path[l++] = L'\\';
            path[l] = L'\0';
        }
        wcsncat(path, base, cap - wcslen(path) - 1);
    }
}

/* ------------------------------------------------------------------ */
/* Post-extraction actions                                             */
/* ------------------------------------------------------------------ */

static void add_to_path(const wchar_t *extract_path)
{
    const char *sub = sfx_settings_get(&g_settings, "path_subfolder", "");
    wchar_t target[MAX_PATH * 2];
    if (sub && sub[0]) {
        wchar_t *ws = sfx_utf8_to_wide(sub);
        swprintf(target, MAX_PATH * 2, L"%ls\\%ls", extract_path, ws);
        free(ws);
    } else {
        wcsncpy(target, extract_path, MAX_PATH * 2 - 1);
        target[MAX_PATH * 2 - 1] = L'\0';
    }

    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0,
                      KEY_READ | KEY_WRITE, &key) != ERROR_SUCCESS)
        return;

    wchar_t current[32768];
    DWORD type = 0, cb = sizeof(current);
    current[0] = L'\0';
    RegQueryValueExW(key, L"PATH", NULL, &type, (LPBYTE)current, &cb);

    if (wcsstr(current, target) == NULL) {
        wchar_t *newval = (wchar_t *)malloc((wcslen(current) + wcslen(target) + 2) * sizeof(wchar_t));
        if (current[0])
            swprintf(newval, wcslen(current) + wcslen(target) + 2, L"%ls;%ls", current, target);
        else
            wcscpy(newval, target);
        RegSetValueExW(key, L"PATH", 0, REG_EXPAND_SZ,
                       (const BYTE *)newval,
                       (DWORD)((wcslen(newval) + 1) * sizeof(wchar_t)));
        free(newval);
        DWORD_PTR res;
        SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                            (LPARAM)L"Environment", SMTO_ABORTIFHUNG, 5000, &res);
    }
    RegCloseKey(key);
}

static int create_shortcut(const wchar_t *lnk_path, const wchar_t *target,
                           const wchar_t *workdir)
{
    IShellLinkW *psl = NULL;
    IPersistFile *ppf = NULL;
    int ok = 0;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IShellLinkW, (void **)&psl))) {
        psl->lpVtbl->SetPath(psl, target);
        psl->lpVtbl->SetWorkingDirectory(psl, workdir);
        if (SUCCEEDED(psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile, (void **)&ppf))) {
            if (SUCCEEDED(ppf->lpVtbl->Save(ppf, lnk_path, TRUE)))
                ok = 1;
            ppf->lpVtbl->Release(ppf);
        }
        psl->lpVtbl->Release(psl);
    }
    return ok;
}

static void create_desktop_shortcut(const wchar_t *extract_path)
{
    const char *tf = sfx_settings_get(&g_settings, "desktop_target_file", "");
    if (!tf || !tf[0])
        return;
    const char *name = sfx_settings_get(&g_settings, "desktop_shortcut_name", "My Application");

    wchar_t *wtf = sfx_utf8_to_wide(tf);
    wchar_t *wname = sfx_utf8_to_wide(name);

    wchar_t target[MAX_PATH * 2];
    swprintf(target, MAX_PATH * 2, L"%ls\\%ls", extract_path, wtf);

    if (GetFileAttributesW(target) != INVALID_FILE_ATTRIBUTES) {
        wchar_t desktop[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop))) {
            wchar_t lnk[MAX_PATH * 2];
            swprintf(lnk, MAX_PATH * 2, L"%ls\\%ls.lnk", desktop, wname);
            create_shortcut(lnk, target, extract_path);
        }
    }
    free(wtf);
    free(wname);
}

static void create_startmenu_shortcut(const wchar_t *extract_path)
{
    const char *tf = sfx_settings_get(&g_settings, "desktop_target_file", "");
    if (!tf || !tf[0])
        return;
    const char *group = sfx_settings_get(&g_settings, "startmenu_group", "My Software");
    const char *name = sfx_settings_get(&g_settings, "startmenu_shortcut_name", "My Application");

    wchar_t *wtf = sfx_utf8_to_wide(tf);
    wchar_t *wgroup = sfx_utf8_to_wide(group);
    wchar_t *wname = sfx_utf8_to_wide(name);

    wchar_t target[MAX_PATH * 2];
    swprintf(target, MAX_PATH * 2, L"%ls\\%ls", extract_path, wtf);

    if (GetFileAttributesW(target) != INVALID_FILE_ATTRIBUTES) {
        wchar_t programs[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, programs))) {
            wchar_t groupdir[MAX_PATH * 2];
            swprintf(groupdir, MAX_PATH * 2, L"%ls\\%ls", programs, wgroup);
            CreateDirectoryW(groupdir, NULL);
            wchar_t lnk[MAX_PATH * 2];
            swprintf(lnk, MAX_PATH * 2, L"%ls\\%ls.lnk", groupdir, wname);
            create_shortcut(lnk, target, extract_path);
        }
    }
    free(wtf);
    free(wgroup);
    free(wname);
}

static void create_registry_entries(const wchar_t *extract_path)
{
    const char *name = sfx_settings_get(&g_settings, "registry_program_name", "My Application");
    const char *ver = sfx_settings_get(&g_settings, "registry_version", "1.0.0");
    const char *pub = sfx_settings_get(&g_settings, "registry_publisher", "My Company");

    wchar_t *wname = sfx_utf8_to_wide(name);
    wchar_t *wver = sfx_utf8_to_wide(ver);
    wchar_t *wpub = sfx_utf8_to_wide(pub);

    wchar_t keypath[MAX_PATH * 2];
    swprintf(keypath, MAX_PATH * 2,
             L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\%ls", wname);

    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, keypath, 0, NULL, 0,
                        KEY_WRITE, NULL, &key, NULL) == ERROR_SUCCESS) {
        RegSetValueExW(key, L"DisplayName", 0, REG_SZ,
                       (const BYTE *)wname, (DWORD)((wcslen(wname) + 1) * sizeof(wchar_t)));
        RegSetValueExW(key, L"DisplayVersion", 0, REG_SZ,
                       (const BYTE *)wver, (DWORD)((wcslen(wver) + 1) * sizeof(wchar_t)));
        RegSetValueExW(key, L"Publisher", 0, REG_SZ,
                       (const BYTE *)wpub, (DWORD)((wcslen(wpub) + 1) * sizeof(wchar_t)));
        RegSetValueExW(key, L"InstallLocation", 0, REG_SZ,
                       (const BYTE *)extract_path,
                       (DWORD)((wcslen(extract_path) + 1) * sizeof(wchar_t)));
        wchar_t uninstall[MAX_PATH * 3];
        swprintf(uninstall, MAX_PATH * 3, L"cmd.exe /c rmdir /s /q \"%ls\"", extract_path);
        RegSetValueExW(key, L"UninstallString", 0, REG_SZ,
                       (const BYTE *)uninstall,
                       (DWORD)((wcslen(uninstall) + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
    free(wname);
    free(wver);
    free(wpub);
}

static void run_post_extraction_file(const wchar_t *extract_path)
{
    const char *rf = sfx_settings_get(&g_settings, "run_file_path", "");
    if (!rf || !rf[0])
        return;
    wchar_t *wrf = sfx_utf8_to_wide(rf);
    wchar_t target[MAX_PATH * 2];
    swprintf(target, MAX_PATH * 2, L"%ls\\%ls", extract_path, wrf);
    if (GetFileAttributesW(target) != INVALID_FILE_ATTRIBUTES) {
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = target;
        sei.lpDirectory = extract_path;
        sei.nShow = SW_SHOWNORMAL;
        ShellExecuteExW(&sei);
    }
    free(wrf);
}

static void do_post_extraction(const wchar_t *extract_path, int silent)
{
    if (sfx_settings_get_bool(&g_settings, "add_to_path", 0))
        add_to_path(extract_path);
    if (sfx_settings_get_bool(&g_settings, "create_desktop_shortcut", 0))
        create_desktop_shortcut(extract_path);
    if (sfx_settings_get_bool(&g_settings, "create_startmenu_shortcut", 0))
        create_startmenu_shortcut(extract_path);
    if (sfx_settings_get_bool(&g_settings, "create_registry_entries", 0))
        create_registry_entries(extract_path);
    if (!silent && sfx_settings_get_bool(&g_settings, "open_folder", 1))
        ShellExecuteW(NULL, L"explore", extract_path, NULL, NULL, SW_SHOWNORMAL);
    if (sfx_settings_get_bool(&g_settings, "run_file", 0))
        run_post_extraction_file(extract_path);
}

/* ------------------------------------------------------------------ */
/* Worker thread                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    wchar_t dest[MAX_PATH * 2];
    char    password[512];
    int     overwrite;
} ExtractJob;

static void gui_progress(void *ctx, int percent, const char *status)
{
    (void)ctx;
    PostMessageW(g_hwnd, WM_APP_PROGRESS, (WPARAM)percent, (LPARAM)_strdup(status));
}

static DWORD WINAPI worker_thread(LPVOID arg)
{
    ExtractJob *job = (ExtractJob *)arg;
    wchar_t err[256];
    err[0] = L'\0';

    int rc = sfx_extract(&g_payload, job->dest,
                         g_payload.encrypted ? job->password : NULL,
                         job->overwrite, gui_progress, NULL, err, 256);
    if (rc == 0) {
        gui_progress(NULL, 90, "Configuring system...");
        do_post_extraction(job->dest, 0);
        gui_progress(NULL, 100, "Extraction completed!");
    }
    PostMessageW(g_hwnd, WM_APP_DONE, (WPARAM)(rc == 0),
                 (LPARAM)(err[0] ? _wcsdup(err) : NULL));
    free(job);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Silent / auto extraction                                            */
/* ------------------------------------------------------------------ */

static int run_silent(const char *password, const wchar_t *dest_override)
{
    wchar_t dest[MAX_PATH * 2];
    if (dest_override && dest_override[0]) {
        wcsncpy(dest, dest_override, MAX_PATH * 2 - 1);
        dest[MAX_PATH * 2 - 1] = L'\0';
    } else {
        resolve_default_path(dest, MAX_PATH * 2);
    }
    apply_subfolder(dest, MAX_PATH * 2);
    sfx_make_dirs(dest);

    wchar_t err[256];
    err[0] = L'\0';
    int rc = sfx_extract(&g_payload, dest,
                         g_payload.encrypted ? password : NULL,
                         sfx_settings_get_bool(&g_settings, "overwrite_files", 1),
                         NULL, NULL, err, 256);
    if (rc == 0)
        do_post_extraction(dest, 1);
    return rc == 0 ? 0 : 1;
}

/* ------------------------------------------------------------------ */
/* GUI                                                                 */
/* ------------------------------------------------------------------ */

static HWND make_label(HWND parent, const wchar_t *text, int x, int y, int w, int h,
                       HFONT font, DWORD extra)
{
    HWND l = CreateWindowExW(0, L"STATIC", text,
                             WS_CHILD | WS_VISIBLE | extra,
                             x, y, w, h, parent, NULL, g_hinst, NULL);
    SendMessageW(l, WM_SETFONT, (WPARAM)font, TRUE);
    return l;
}

static void start_extraction(void)
{
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0)
        return;

    ExtractJob *job = (ExtractJob *)calloc(1, sizeof(ExtractJob));
    GetWindowTextW(g_path_edit, job->dest, MAX_PATH * 2);
    if (job->dest[0] == L'\0') {
        MessageBoxW(g_hwnd, L"Please choose an extraction folder.",
                    L"Zipline", MB_OK | MB_ICONWARNING);
        free(job);
        g_running = 0;
        return;
    }
    apply_subfolder(job->dest, MAX_PATH * 2);
    sfx_make_dirs(job->dest);

    job->overwrite = sfx_settings_get_bool(&g_settings, "overwrite_files", 1);

    if (g_has_password_ui) {
        wchar_t wpass[256];
        GetWindowTextW(g_pass_edit, wpass, 256);
        WideCharToMultiByte(CP_UTF8, 0, wpass, -1, job->password,
                            sizeof(job->password), NULL, NULL);
    }

    EnableWindow(g_extract_btn, FALSE);
    SetWindowTextW(g_status, L"Extracting...");

    HANDLE th = CreateThread(NULL, 0, worker_thread, job, 0, NULL);
    if (th) CloseHandle(th);
}

static void browse_folder(void)
{
    wchar_t current[MAX_PATH * 2];
    GetWindowTextW(g_path_edit, current, MAX_PATH * 2);

    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = L"Select extraction folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        wchar_t path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path))
            SetWindowTextW(g_path_edit, path);
        CoTaskMemFree(pidl);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        int cx = 560;
        int y = 20;

        /* window title / description from settings */
        const char *title = sfx_settings_get(&g_settings, "window_title",
                                              "Zipline Self-Extracting Archive");
        const char *desc = sfx_settings_get(&g_settings, "description",
            "This archive will extract files to a folder you choose.\n"
            "Click 'Extract' to select a destination and begin extraction.");
        wchar_t *wtitle = sfx_utf8_to_wide(title);
        wchar_t *wdesc = sfx_utf8_to_wide(desc);

        y = 90;
        make_label(hwnd, wtitle, 20, y, cx - 40, 30, g_font_title, SS_CENTER);
        y += 40;
        make_label(hwnd, wdesc, 30, y, cx - 60, 50, g_font, SS_CENTER);
        y += 64;

        make_label(hwnd, L"Extract to:", 30, y, 120, 22, g_font_bold, 0);
        y += 26;
        g_path_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                      30, y, cx - 160, 28, hwnd, (HMENU)101, g_hinst, NULL);
        SendMessageW(g_path_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
        CreateWindowExW(0, L"BUTTON", L"Browse...",
                        WS_CHILD | WS_VISIBLE,
                        cx - 120, y, 90, 28, hwnd, (HMENU)102, g_hinst, NULL);
        {
            HWND b = GetDlgItem(hwnd, 102);
            SendMessageW(b, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        y += 40;

        if (g_payload.encrypted) {
            g_has_password_ui = 1;
            make_label(hwnd, L"Password:", 30, y, 120, 22, g_font_bold, 0);
            y += 26;
            g_pass_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                          WS_CHILD | WS_VISIBLE | ES_PASSWORD,
                                          30, y, cx - 60, 28, hwnd, (HMENU)103, g_hinst, NULL);
            SendMessageW(g_pass_edit, WM_SETFONT, (WPARAM)g_font, TRUE);
            y += 40;
        }

        g_progress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
                                     WS_CHILD | WS_VISIBLE,
                                     30, y, cx - 60, 22, hwnd, (HMENU)104, g_hinst, NULL);
        SendMessageW(g_progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        y += 32;

        g_status = make_label(hwnd, L"Ready to extract", 30, y, cx - 60, 22, g_font, SS_CENTER);
        y += 36;

        g_extract_btn = CreateWindowExW(0, L"BUTTON", L"Extract",
                                        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                        cx / 2 - 170, y, 150, 40, hwnd, (HMENU)105, g_hinst, NULL);
        SendMessageW(g_extract_btn, WM_SETFONT, (WPARAM)g_font_bold, TRUE);
        g_cancel_btn = CreateWindowExW(0, L"BUTTON", L"Cancel",
                                       WS_CHILD | WS_VISIBLE,
                                       cx / 2 + 20, y, 150, 40, hwnd, (HMENU)106, g_hinst, NULL);
        SendMessageW(g_cancel_btn, WM_SETFONT, (WPARAM)g_font_bold, TRUE);

        wchar_t defpath[MAX_PATH * 2];
        resolve_default_path(defpath, MAX_PATH * 2);
        SetWindowTextW(g_path_edit, defpath);

        free(wtitle);
        free(wdesc);
        return 0;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, RGB(255, 255, 255));
        SetBkMode(hdc, OPAQUE);
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    }

    case WM_ERASEBKGND: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));
        /* draw the app icon centred near the top */
        if (g_app_icon)
            DrawIconEx((HDC)wp, (rc.right - 64) / 2, 16, g_app_icon,
                       64, 64, 0, NULL, DI_NORMAL);
        return 1;
    }

    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT di = (LPDRAWITEMSTRUCT)lp;
        if (di->CtlID == 105) {
            int pressed = (di->itemState & ODS_SELECTED);
            int disabled = (di->itemState & ODS_DISABLED);
            COLORREF bg = disabled ? RGB(170, 200, 240)
                                   : (pressed ? RGB(0, 86, 204) : RGB(0, 122, 255));
            HBRUSH br = CreateSolidBrush(bg);
            FillRect(di->hDC, &di->rcItem, br);
            DeleteObject(br);
            SetBkMode(di->hDC, TRANSPARENT);
            SetTextColor(di->hDC, RGB(255, 255, 255));
            SelectObject(di->hDC, g_font_bold);
            wchar_t txt[64];
            GetWindowTextW(di->hwndItem, txt, 64);
            DrawTextW(di->hDC, txt, -1, &di->rcItem,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case 102: browse_folder(); return 0;
        case 105: start_extraction(); return 0;
        case 106: if (!g_running) DestroyWindow(hwnd); return 0;
        }
        return 0;

    case WM_APP_PROGRESS: {
        SendMessageW(g_progress, PBM_SETPOS, (WPARAM)wp, 0);
        char *status = (char *)lp;
        if (status) {
            wchar_t *ws = sfx_utf8_to_wide(status);
            SetWindowTextW(g_status, ws);
            free(ws);
            free(status);
        }
        return 0;
    }

    case WM_APP_DONE: {
        g_running = 0;
        EnableWindow(g_extract_btn, TRUE);
        int ok = (int)wp;
        wchar_t *err = (wchar_t *)lp;
        if (ok) {
            wchar_t dest[MAX_PATH * 2];
            GetWindowTextW(g_path_edit, dest, MAX_PATH * 2);
            MessageBoxW(hwnd, L"Files extracted successfully!",
                        L"Success", MB_OK | MB_ICONINFORMATION);
        } else {
            SetWindowTextW(g_status, L"Extraction failed");
            SendMessageW(g_progress, PBM_SETPOS, 0, 0);
            MessageBoxW(hwnd, err ? err : L"Extraction failed.",
                        L"Error", MB_OK | MB_ICONERROR);
        }
        if (err) free(err);
        return 0;
    }

    case WM_CLOSE:
        if (g_running) return 0; /* don't close mid-extraction */
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------------ */
/* Elevation                                                           */
/* ------------------------------------------------------------------ */

static int is_elevated(void)
{
    HANDLE token = NULL;
    TOKEN_ELEVATION elev;
    DWORD cb = sizeof(elev);
    int result = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &cb))
            result = elev.TokenIsElevated;
        CloseHandle(token);
    }
    return result;
}

static int relaunch_elevated(void)
{
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(NULL, self, MAX_PATH);
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = PathGetArgsW(GetCommandLineW());
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    (void)hPrev; (void)lpCmd; (void)nShow;
    g_hinst = hInst;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    InitCommonControls();

    if (read_self_payload() != 0) {
        MessageBoxW(NULL,
                    L"This file does not contain a valid Zipline archive payload.",
                    L"Zipline", MB_OK | MB_ICONERROR);
        return 2;
    }

    /* parse command line */
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    int silent = 0;
    char password[512];
    password[0] = '\0';
    wchar_t dest_override[MAX_PATH * 2];
    dest_override[0] = L'\0';
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"/S") == 0 || _wcsicmp(argv[i], L"-S") == 0 ||
            _wcsicmp(argv[i], L"--silent") == 0) {
            silent = 1;
        } else if (_wcsnicmp(argv[i], L"/P:", 3) == 0) {
            WideCharToMultiByte(CP_UTF8, 0, argv[i] + 3, -1, password,
                                sizeof(password), NULL, NULL);
        } else if (_wcsnicmp(argv[i], L"/D=", 3) == 0) {
            wcsncpy(dest_override, argv[i] + 3, MAX_PATH * 2 - 1);
            dest_override[MAX_PATH * 2 - 1] = L'\0';
        }
    }
    if (argv) LocalFree(argv);

    if (password[0] == '\0') {
        char envpass[512];
        DWORD n = GetEnvironmentVariableA("ZIPLINE_SFX_PASSWORD", envpass, sizeof(envpass));
        if (n > 0 && n < sizeof(envpass)) {
            strncpy(password, envpass, sizeof(password) - 1);
            password[sizeof(password) - 1] = '\0';
        }
    }

    /* elevation if requested */
    if (sfx_settings_get_bool(&g_settings, "require_admin", 0) && !is_elevated()) {
        if (relaunch_elevated())
            return 0;
    }

    int auto_extract = sfx_settings_get_bool(&g_settings, "auto_extract", 0);

    if (silent || auto_extract) {
        int rc = run_silent(password, dest_override[0] ? dest_override : NULL);
        return rc;
    }

    /* fonts & icon */
    g_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                         0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_font_bold = CreateFontW(-16, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                              0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_font_title = CreateFontW(-26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                               0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_app_icon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"ZiplineSfxWindow";
    wc.hIcon = g_app_icon;
    wc.hIconSm = g_app_icon;
    RegisterClassExW(&wc);

    const char *title = sfx_settings_get(&g_settings, "window_title",
                                         "Zipline Self-Extracting Archive");
    wchar_t *wtitle = sfx_utf8_to_wide(title);

    int W = 560, H = g_payload.encrypted ? 560 : 500;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, wtitle,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             (sw - W) / 2, (sh - H) / 2, W, H,
                             NULL, NULL, hInst, NULL);
    free(wtitle);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return 0;
}
