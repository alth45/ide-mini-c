/*
 * main.c  -  Entry point, jendela utama, menu, accelerator, splitter
 */
#include "app.h"
#include <shlobj.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(linker, \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' " \
    "language='*'\"")

/* ---------- Global state ---------- */
Tab       g_tabs[MAX_TABS];
int       g_tabCount = 0;
HWND      g_hMain, g_hTree, g_hTab, g_hSplit;
int       g_splitX = 260;
HFONT     g_hMonoFont = NULL;
HACCEL    g_hAccel = NULL;
BOOL      g_dragging = FALSE;

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
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)w;
        RECT rc;
        GetClientRect(h, &rc);
        HBRUSH b = CreateSolidBrush(theme_palette()->splitter);
        FillRect(hdc, &rc, b);
        DeleteObject(b);
        return 1;
    }
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

        /* TCS_OWNERDRAWFIXED: tab digambar sendiri supaya bisa
           dark mode + tombol close saat hover (lihat theme.c) */
        g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"",
            WS_CHILD | WS_VISIBLE | TCS_HOTTRACK | TCS_FOCUSNEVER |
            TCS_OWNERDRAWFIXED,
            0, 0, 800, TABSTRIP_H, h, NULL, hi, NULL);

        g_hSplit = CreateWindowExW(0, L"MiniIdeSplit", L"",
            WS_CHILD | WS_VISIBLE, 260, 0, SPLIT_W, 600, h, NULL, hi, NULL);

        g_hMonoFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        theme_install_tab_subclass(g_hTab);

        do_layout();
        theme_apply(h);
        break;
    }

    case WM_SIZE:
        do_layout();
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)w;
        HWND he = (HWND)l;
        const ThemePalette *pal = theme_palette();
        /* Warna teks/background control EDIT (editor) mengikuti tema */
        for (int i = 0; i < g_tabCount; i++) {
            if (g_tabs[i].hEdit == he) {
                SetTextColor(hdc, pal->editFg);
                SetBkColor(hdc, pal->editBg);
                return (LRESULT)theme_edit_brush(he);
            }
        }
        break;
    }

    /* Deteksi perubahan isi editor -> tandai tab dirty (● di judul) */
    case WM_COMMAND:
        if (HIWORD(w) == EN_CHANGE && LOWORD(w) == 0) {
            tabs_check_dirty((HWND)l);
            return 0;
        }
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
        case IDM_TOGGLE_DARK: {
            g_darkMode = !g_darkMode;
            CheckMenuItem(GetMenu(h), IDM_TOGGLE_DARK,
                          g_darkMode ? MF_CHECKED : MF_UNCHECKED);
            theme_apply(h);
            return 0;
        }
        case IDM_ABOUT:
            MessageBoxW(h,
                L"Mini IDE (Win32 C)\n\n"
                L"- Arahkan mouse ke tab untuk memunculkan tombol close (\215)\n"
                L"- Klik-tengah pada tab = tutup langsung\n"
                L"- Ctrl+D untuk beralih Dark/Light mode",
                L"Tentang Mini IDE", MB_OK | MB_ICONINFORMATION);
            return 0;
        case IDM_EXIT:
            DestroyWindow(h);
            return 0;
        }
        break;
        
    case WM_DRAWITEM:
        if (theme_handle_drawitem((LPDRAWITEMSTRUCT)l)) return TRUE;
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

    case WM_SETTINGCHANGE:
        /* sinkronkan ulang bila user mengganti tema sistem */
        theme_on_settingchange(h, w, l);
        return 0;

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

    /* Menu File */
    HMENU hFile = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING, IDM_OPEN_FOLDER, L"&Open Folder\tCtrl+O");
    AppendMenuW(hFile, MF_STRING, IDM_SAVE,        L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING, IDM_CLOSE_TAB,   L"&Close Tab\tCtrl+W");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING, IDM_EXIT,        L"E&xit");

    /* Menu View (dark mode) */
    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, MF_STRING, IDM_TOGGLE_DARK, L"&Dark Mode\tCtrl+D");

    HMENU hMenu = CreateMenu();
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFile, L"&File");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hView, L"&View");

    /* Menu Help */
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"&About Mini IDE");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    HWND hwnd = CreateWindowExW(0, L"MiniIdeMain", L"Mini IDE (Windows)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 720,
        NULL, hMenu, hi, NULL);

    ACCEL accel[] = {
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
        { FVIRTKEY | FCONTROL, 'W', IDM_CLOSE_TAB },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN_FOLDER },
        { FVIRTKEY | FCONTROL, 'D', IDM_TOGGLE_DARK },
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
