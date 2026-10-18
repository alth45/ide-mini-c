#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

#define MAX_TABS     64
#define SIDEBAR_MIN  100

enum { IDM_OPEN_FOLDER = 1001, IDM_SAVE, IDM_CLOSE_TAB, IDM_EXIT };

typedef struct {
    HWND  hEdit;
    WCHAR path[MAX_PATH];
} Tab;

typedef struct PathNode {
    WCHAR *path;
    struct PathNode *next;
} PathNode;

static Tab       g_tabs[MAX_TABS];
static int       g_tabCount = 0;
static HWND      g_hMain, g_hTree, g_hTab, g_hSplit;
static int       g_splitX = 260;
static HFONT     g_hMonoFont = NULL;
static PathNode *g_pathList = NULL;
static HACCEL    g_hAccel = NULL;
static BOOL      g_dragging = FALSE;

/* ---------- Utilitas ---------- */
static WCHAR *track_path(const WCHAR *p) {
    WCHAR *c = _wcsdup(p);
    PathNode *n = (PathNode *)malloc(sizeof(PathNode));
    n->path = c; n->next = g_pathList; g_pathList = n;
    return c;
}

static void free_paths(void) {
    while (g_pathList) {
        PathNode *n = g_pathList;
        g_pathList = n->next;
        free(n->path);
        free(n);
    }
}

static void path_join(WCHAR *out, DWORD outSize, const WCHAR *a, const WCHAR *b) {
    lstrcpynW(out, a, outSize);
    size_t len = wcslen(out);
    if (len + 1 < outSize) {
        out[len++] = L'\\';
        lstrcpynW(out + len, b, outSize - (int)len);
    }
}

/* ---------- Layout ---------- */
static void do_layout(void) {
    if (!g_hMain || !g_hTree || !g_hTab || !g_hSplit) return;
    RECT rc;
    GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom;
    if (g_splitX < SIDEBAR_MIN) g_splitX = SIDEBAR_MIN;
    if (g_splitX > W - 150)    g_splitX = W - 150;

    MoveWindow(g_hTree,  0,           0, g_splitX, H, TRUE);
    MoveWindow(g_hSplit, g_splitX,    0, 4,        H, TRUE);

    int ex = g_splitX + 4;
    int ew = W - ex;
    MoveWindow(g_hTab, ex, 0, ew, 26, TRUE);
    for (int i = 0; i < g_tabCount; i++)
        MoveWindow(g_tabs[i].hEdit, ex, 26, ew, H - 26, TRUE);
}

static void show_tab(int idx) {
    for (int i = 0; i < g_tabCount; i++)
        ShowWindow(g_tabs[i].hEdit, i == idx ? SW_SHOW : SW_HIDE);
    if (idx >= 0 && idx < g_tabCount) SetFocus(g_tabs[idx].hEdit);
}

/* ---------- File I/O ---------- */
static int find_tab(const WCHAR *path) {
    for (int i = 0; i < g_tabCount; i++)
        if (_wcsicmp(g_tabs[i].path, path) == 0) return i;
    return -1;
}

static WCHAR *read_text_file(const WCHAR *path) {
    HANDLE h = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;

    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE) { CloseHandle(h); return NULL; }
    char *buf = (char *)malloc(size + 1);
    DWORD read = 0;
    ReadFile(h, buf, size, &read, NULL);
    CloseHandle(h);
    buf[read] = 0;

    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, buf, read, NULL, 0);
    UINT cp = CP_UTF8;
    if (wlen == 0) {
        cp = CP_ACP;
        wlen = MultiByteToWideChar(cp, 0, buf, read, NULL, 0);
    }
    WCHAR *out = (WCHAR *)malloc((wlen + 1) * sizeof(WCHAR));
    MultiByteToWideChar(cp, 0, buf, read, out, wlen);
    out[wlen] = 0;
    free(buf);
    return out;
}

static BOOL save_tab(int idx) {
    if (idx < 0 || idx >= g_tabCount) return FALSE;
    Tab *t = &g_tabs[idx];
    int len = GetWindowTextLengthW(t->hEdit);
    WCHAR *wbuf = (WCHAR *)malloc((len + 1) * sizeof(WCHAR));
    GetWindowTextW(t->hEdit, wbuf, len + 1);

    int clen = WideCharToMultiByte(CP_UTF8, 0, wbuf, len, NULL, 0, NULL, NULL);
    char *buf = (char *)malloc(clen);
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
    free(wbuf); free(buf);
    return ok;
}

/* ---------- Tab ---------- */
static void open_file_in_tab(const WCHAR *path) {
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
    lstrcpynW(t->path, path, MAX_PATH);

    const WCHAR *base = wcsrchr(path, L'\\');
    if (!base) base = wcsrchr(path, L'/');
    base = base ? base + 1 : path;

    TCITEMW tie = {0};
    tie.mask   = TCIF_TEXT;
    tie.pszText = (LPWSTR)base;
    TabCtrl_InsertItem(g_hTab, g_tabCount, &tie);
    g_tabCount++;

    TabCtrl_SetCurSel(g_hTab, g_tabCount - 1);
    do_layout();
    show_tab(g_tabCount - 1);
}

static void close_tab(int idx) {
    if (idx < 0 || idx >= g_tabCount) return;
    DestroyWindow(g_tabs[idx].hEdit);
    for (int i = idx; i < g_tabCount - 1; i++) g_tabs[i] = g_tabs[i + 1];
    g_tabCount--;
    TabCtrl_DeleteItem(g_hTab, idx);
    if (g_tabCount > 0) {
        int newIdx = (idx < g_tabCount) ? idx : g_tabCount - 1;
        TabCtrl_SetCurSel(g_hTab, newIdx);
        show_tab(newIdx);
    }
}

/* ---------- Tree ---------- */
static void populate_tree(HWND hTree, HTREEITEM parent, const WCHAR *dir) {
    WCHAR pattern[MAX_PATH];
    lstrcpynW(pattern, dir, MAX_PATH);
    if (wcslen(pattern) + 3 < MAX_PATH) wcscat(pattern, L"\\*");

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    /* dirs first */
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (wcscmp(fd.cFileName, L".git") == 0 ||
            wcscmp(fd.cFileName, L"node_modules") == 0 ||
            wcscmp(fd.cFileName, L"build") == 0 ||
            wcscmp(fd.cFileName, L"dist") == 0 ||
            wcscmp(fd.cFileName, L"__pycache__") == 0) continue;

        WCHAR full[MAX_PATH];
        path_join(full, MAX_PATH, dir, fd.cFileName);

        TVINSERTSTRUCTW tv = {0};
        tv.hParent      = parent ? parent : TVI_ROOT;
        tv.hInsertAfter = TVI_LAST;
        tv.item.mask    = TVIF_TEXT | TVIF_PARAM;
        tv.item.pszText = fd.cFileName;
        tv.item.lParam  = (LPARAM)track_path(full);
        HTREEITEM hItem = TreeView_InsertItem(hTree, &tv);
        populate_tree(hTree, hItem, full);
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        WCHAR full[MAX_PATH];
        path_join(full, MAX_PATH, dir, fd.cFileName);

        TVINSERTSTRUCTW tv = {0};
        tv.hParent      = parent ? parent : TVI_ROOT;
        tv.hInsertAfter = TVI_LAST;
        tv.item.mask    = TVIF_TEXT | TVIF_PARAM;
        tv.item.pszText = fd.cFileName;
        tv.item.lParam  = (LPARAM)track_path(full);
        TreeView_InsertItem(hTree, &tv);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static void open_folder(const WCHAR *folder) {
    TreeView_DeleteAllItems(g_hTree);
    free_paths();
    populate_tree(g_hTree, NULL, folder);
}

static BOOL pick_folder(WCHAR *out, DWORD size) {
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"Pilih folder project";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return FALSE;
    BOOL ok = SHGetPathFromIDListW(pidl, out);
    CoTaskMemFree(pidl);
    return ok;
}

/* ---------- Splitter window ---------- */
static LRESULT CALLBACK SplitProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_LBUTTONDOWN:
        g_dragging = TRUE;
        SetCapture(h);
        return 0;
    case WM_LBUTTONUP:
        g_dragging = FALSE;
        ReleaseCapture();
        return 0;
    case WM_MOUSEMOVE:
        if (g_dragging) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(g_hMain, &pt);
            g_splitX = pt.x;
            do_layout();
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---------- Main window proc ---------- */
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_hMain = h;
        HINSTANCE hi = GetModuleHandleW(NULL);

        g_hTree = CreateWindowExW(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
            WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_HASBUTTONS |
            TVS_LINESATROOT | TVS_SHOWSELALWAYS,
            0, 0, 260, 600, h, NULL, hi, NULL);

        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_HOTTRACK | TCS_FOCUSNEVER,
            0, 0, 800, 26, h, NULL, hi, NULL);

        g_hSplit = CreateWindowExW(0, L"MiniIdeSplit", L"",
            WS_CHILD | WS_VISIBLE, 260, 0, 4, 600, h, NULL, hi, NULL);

        g_hMonoFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        do_layout();
        break;
    }
    case WM_SIZE:
        do_layout();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDM_OPEN_FOLDER: {
            WCHAR folder[MAX_PATH];
            if (pick_folder(folder, MAX_PATH)) open_folder(folder);
            return 0;
        }
        case IDM_SAVE: {
            int sel = TabCtrl_GetCurSel(g_hTab);
            if (sel >= 0) save_tab(sel);
            return 0;
        }
        case IDM_CLOSE_TAB: {
            int sel = TabCtrl_GetCurSel(g_hTab);
            close_tab(sel);
            return 0;
        }
        case IDM_EXIT:
            DestroyWindow(h);
            return 0;
        }
        break;

    case WM_NOTIFY: {
        NMHDR *nh = (NMHDR *)l;
        if (nh->hwndFrom == g_hTab && nh->code == TCN_SELCHANGE) {
            show_tab(TabCtrl_GetCurSel(g_hTab));
            return 0;
        }
        if (nh->hwndFrom == g_hTree && nh->code == NM_DBLCLK) {
            HTREEITEM sel = TreeView_GetSelection(g_hTree);
            if (sel) {
                TVITEMW tv = {0};
                tv.mask  = TVIF_PARAM;
                tv.hItem = sel;
                if (TreeView_GetItem(g_hTree, &tv) && tv.lParam)
                    open_file_in_tab((WCHAR *)tv.lParam);
            }
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---------- Entry point ---------- */
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE hp, PWSTR cmd, int show) {
    (void)hp; (void)cmd;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    INITCOMMONCONTROLSEX icc = {
        sizeof(icc),
        ICC_TREEVIEW_CLASSES | ICC_TAB_CLASSES | ICC_STANDARD_CLASSES
    };
    InitCommonControlsEx(&icc);

    WNDCLASSW wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hi;
    wc.lpszClassName = L"MiniIdeMain";
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassW(&wc);

    WNDCLASSW sc = {0};
    sc.lpfnWndProc   = SplitProc;
    sc.hInstance     = hi;
    sc.lpszClassName = L"MiniIdeSplit";
    sc.hCursor       = LoadCursorW(NULL, IDC_SIZEWE);
    sc.hbrBackground = (HBRUSH)(COLOR_BTNSHADOW + 1);
    RegisterClassW(&sc);

    /* Menu */
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_OPEN_FOLDER, L"&Open Folder\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_SAVE,        L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_CLOSE_TAB,   L"&Close Tab\tCtrl+W");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_EXIT,        L"E&xit");

    HMENU hMenu = CreateMenu();
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");

    HWND hwnd = CreateWindowExW(0, L"MiniIdeMain", L"Mini IDE (Windows)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720,
        NULL, hMenu, hi, NULL);

    ACCEL accel[] = {
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
        { FVIRTKEY | FCONTROL, 'W', IDM_CLOSE_TAB },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN_FOLDER },
    };
    g_hAccel = CreateAcceleratorTableW(accel, sizeof(accel) / sizeof(accel[0]));

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hwnd, g_hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    free_paths();
    if (g_hMonoFont) DeleteObject(g_hMonoFont);
    if (g_hAccel)    DestroyAcceleratorTable(g_hAccel);
    CoUninitialize();
    return 0;
}