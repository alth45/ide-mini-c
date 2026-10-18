/*
 * tabs.c  -  Manajemen tab: buka, tutup, judul (dengan penanda dirty)
 */
#include "app.h"

int find_tab(const WCHAR *path) {
    for (int i = 0; i < g_tabCount; i++)
        if (_wcsicmp(g_tabs[i].path, path) == 0) return i;
    return -1;
}

static const WCHAR *tab_basename(const WCHAR *path) {
    const WCHAR *base = wcsrchr(path, L'\\');
    if (!base) base = wcsrchr(path, L'/');
    return base ? base + 1 : path;
}

void update_tab_title(int idx) {
    if (idx < 0 || idx >= g_tabCount || !g_hTab) return;

    WCHAR title[MAX_PATH + 4];
    lstrcpynW(title, tab_basename(g_tabs[idx].path), MAX_PATH);
    if (g_tabs[idx].dirty) {
        size_t n = wcslen(title);
        if (n + 2 < sizeof(title) / sizeof(title[0])) {
            title[n] = L' ';
            title[n + 1] = L'\226';   /* ● menandakan belum disimpan */
            title[n + 2] = 0;
        }
    }

    TCITEMW tie = {0};
    tie.mask    = TCIF_TEXT;
    tie.pszText = title;
    TabCtrl_SetItem(g_hTab, idx, &tie);
    InvalidateRect(g_hTab, NULL, TRUE);
}

void open_file_in_tab(const WCHAR *path) {
    int idx = find_tab(path);
    if (idx >= 0) {
        TabCtrl_SetCurSel(g_hTab, idx);
        show_tab(idx);
        return;
    }
    if (g_tabCount >= MAX_TABS) return;

    DWORD attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        return;

    WCHAR *content = read_text_file(path);
    if (!content) return;

    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", content,
        WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
        0, 0, 0, 0, g_hMain, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(hEdit, WM_SETFONT, (WPARAM)g_hMonoFont, TRUE);
    free(content);

    Tab *t = &g_tabs[g_tabCount];
    t->hEdit = hEdit;
    t->dirty = FALSE;
    t->closeBtnVisible = FALSE;
    t->closeBtnHovered = FALSE;
    SetRectEmpty(&t->closeBtn);
    lstrcpynW(t->path, path, MAX_PATH);

    TCITEMW tie = {0};
    tie.mask    = TCIF_TEXT;
    tie.pszText = (LPWSTR)tab_basename(path);
    TabCtrl_InsertItem(g_hTab, g_tabCount, &tie);
    g_tabCount++;

    TabCtrl_SetCurSel(g_hTab, g_tabCount - 1);
    do_layout();
    show_tab(g_tabCount - 1);
}

void close_tab(int idx) {
    if (idx < 0 || idx >= g_tabCount) return;

    /* tanyakan bila ada perubahan belum disimpan */
    if (g_tabs[idx].dirty) {
        WCHAR msg[512];
        _snwprintf(msg, 510,
            L"\"\\215\" %s masih belum disimpan.\n\nTutup tanpa menyimpan?",
            tab_basename(g_tabs[idx].path));
        int r = MessageBoxW(g_hMain, msg, L"Mini IDE",
                            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        if (r == IDNO) return;
    }
    force_close_tab(idx);
}

/* Tutup tab tanpa konfirmasi (dipakai klik-tengah / aksi cepat) */
void force_close_tab(int idx) {
    if (idx < 0 || idx >= g_tabCount) return;

    DestroyWindow(g_tabs[idx].hEdit);
    for (int i = idx; i < g_tabCount - 1; i++) g_tabs[i] = g_tabs[i + 1];
    g_tabCount--;

    TabCtrl_DeleteItem(g_hTab, idx);

    if (g_tabCount > 0) {
        int newIdx = (idx < g_tabCount) ? idx : g_tabCount - 1;
        TabCtrl_SetCurSel(g_hTab, newIdx);
        show_tab(newIdx);
    } else {
        show_tab(-1);
    }
    do_layout();
}

/* Dipanggil dari WM_COMMAND EN_CHANGE di main.c: tandai tab dirty */
void tabs_check_dirty(HWND hEdit) {
    for (int i = 0; i < g_tabCount; i++) {
        if (g_tabs[i].hEdit == hEdit) {
            if (!g_tabs[i].dirty) {
                g_tabs[i].dirty = TRUE;
                update_tab_title(i);
            }
            return;
        }
    }
}

/* Sinkronkan state hover setelah tab berpindah/ditutup dari luar */
void tabs_notify_switch(void) {
    if (g_hTab) InvalidateRect(g_hTab, NULL, TRUE);
}
