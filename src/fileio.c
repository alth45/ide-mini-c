/*
 * fileio.c  -  Baca/tulis file teks (UTF-8 dengan fallback ACP)
 */
#include "app.h"

WCHAR *read_text_file(const WCHAR *path) {
    HANDLE h = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) { CloseHandle(h); return NULL; }

    char *buf = (char *)malloc(size + 1);
    if (!buf) { CloseHandle(h); return NULL; }
    DWORD read = 0;
    ReadFile(h, buf, size, &read, NULL);
    CloseHandle(h);
    buf[read] = 0;

    /* buang BOM UTF-8 bila ada */
    int off = 0;
    if (read >= 3 && (unsigned char)buf[0] == 0xEF &&
        (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        off = 3;

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   buf + off, read - off, NULL, 0);
    UINT cp = CP_UTF8;
    if (wlen == 0) {
        cp = CP_ACP;
        wlen = MultiByteToWideChar(cp, 0, buf + off, read - off, NULL, 0);
    }

    WCHAR *out = (WCHAR *)malloc((wlen + 1) * sizeof(WCHAR));
    if (out) {
        MultiByteToWideChar(cp, 0, buf + off, read - off, out, wlen);
        out[wlen] = 0;
    }
    free(buf);
    return out;
}

BOOL save_tab(int idx) {
    if (idx < 0 || idx >= g_tabCount) return FALSE;
    Tab *t = &g_tabs[idx];

    int len = GetWindowTextLengthW(t->hEdit);
    WCHAR *wbuf = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    if (!wbuf) return FALSE;
    GetWindowTextW(t->hEdit, wbuf, len + 1);

    int clen = WideCharToMultiByte(CP_UTF8, 0, wbuf, len, NULL, 0, NULL, NULL);
    char *buf = (char *)malloc(clen > 0 ? clen : 1);
    if (buf)
        WideCharToMultiByte(CP_UTF8, 0, wbuf, len, buf, clen, NULL, NULL);

    HANDLE h = CreateFileW(t->path, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BOOL ok = FALSE;
    if (h != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(h, buf, clen, &written, NULL);
        CloseHandle(h);
        ok = TRUE;
    }
    free(wbuf);
    free(buf);

    if (ok) {
        t->dirty = FALSE;
        update_tab_title(idx);
    }
    return ok;
}
