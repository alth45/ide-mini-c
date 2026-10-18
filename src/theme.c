/*
 * theme.c  -  Dark mode + tombol close pada tab saat di-hover
 *
 * Fitur:
 * 1) Dark mode
 *    - "Immersive dark mode" utk title bar & menu (Win10/11) via
 *      DwmSetWindowAttribute.
 *    - Editor (control EDIT) diberi warna teks/background lewat WM_CTLCOLOR*.
 *    - Tab strip digambar ulang (owner-draw) dengan warna tema.
 *    - TreeView sidebar ikut diwarnai sesuai tema.
 *
 * 2) Tombol close per-tab saat hover
 *    - Tab control di-subclass; saat mouse berada di atas sebuah tab,
 *      tanda "×" muncul di sisi kanan tab tersebut.
 *    - Bila kursor persis di atas kotak "×", kotak itu di-highlight.
 *    - Klik kiri pada "×" menutup tab; klik-tengah pada tab langsung
 *      menutup tanpa konfirmasi (gaya browser).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include "app.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(l) ((int)(short)LOWORD(l))
#define GET_Y_LPARAM(l) ((int)(short)HIWORD(l))
#endif

BOOL g_darkMode = FALSE;   /* status dark mode (dipakai seluruh modul) */

/* ---------- Palet warna (dark / light) ---------- */
static const ThemePalette g_palDark = {
    /* winBg     */ RGB( 37,  37,  38),
    /* text      */ RGB(220, 220, 220),
    /* textDim   */ RGB(150, 150, 153),
    /* editBg    */ RGB( 30,  30,  30),
    /* editFg    */ RGB(212, 212, 212),
    /* tabSelBg  */ RGB( 24,  24,  24),
    /* tabHotBg  */ RGB( 62,  62,  64),
    /* tabIdleBg */ RGB( 45,  45,  48),
    /* closeBg   */ RGB( 86,  86,  90),
    /* closeHover*/ RGB(232,  17,  35),   /* merah, seperti VS Code */
    /* accent    */ RGB(  0, 122, 204),
    /* splitter  */ RGB( 15,  15,  15)
};

static const ThemePalette g_palLight = {
    /* winBg     */ RGB(240, 240, 240),
    /* text      */ RGB(  0,   0,   0),
    /* textDim   */ RGB(110, 110, 115),
    /* editBg    */ RGB(255, 255, 255),
    /* editFg    */ RGB(  0,   0,   0),
    /* tabSelBg  */ RGB(255, 255, 255),
    /* tabHotBg  */ RGB(210, 210, 214),
    /* tabIdleBg */ RGB(233, 233, 233),
    /* closeBg   */ RGB(178, 178, 183),
    /* closeHover*/ RGB(209,  52,  56),
    /* accent    */ RGB(  0, 102, 197),
    /* splitter  */ RGB(160, 160, 160)
};

const ThemePalette *theme_palette(void) {
    return g_darkMode ? &g_palDark : &g_palLight;
}

static WNDPROC g_origTabProc  = NULL;   /* subclass TABWINDOW */
static HBRUSH  g_editBrush    = NULL;   /* brush background editor */
static int     g_hoverTab     = -1;     /* index tab yang sedang di-hover */
static int     g_closeHot     = -1;     /* tab yg tombol ×-nya di-hover presisi */

#define CLOSE_BTN_SZ  14   /* ukuran kotak tombol ×      */
#define CLOSE_BTN_PAD  5   /* jarak dari tepi kanan tab  */

/* ---------- Warna tema (API ringkas) ---------- */
COLORREF theme_tab_bg(void)  { return theme_palette()->winBg; }
COLORREF theme_tab_fg(void)  { return theme_palette()->text; }
COLORREF theme_edit_fg(void) { return theme_palette()->editFg; }

static void ensure_brushes(void) {
    if (g_editBrush) DeleteObject(g_editBrush);
    g_editBrush = CreateSolidBrush(theme_palette()->editBg);
}

HBRUSH theme_edit_brush(HWND hEdit) {
    (void)hEdit;
    if (!g_editBrush) ensure_brushes();
    return g_editBrush;
}

/* Immersive dark mode utk title bar + menu utama */
LRESULT theme_on_settingchange(HWND h, WPARAM w, LPARAM l) {
    (void)w; (void)l;
    BOOL dark = g_darkMode;
    DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DrawMenuBar(h);
    InvalidateRect(h, NULL, TRUE);
    return 0;
}

/* ---------- Helper geometri tab ---------- */
static BOOL get_item_rect(HWND hTab, int idx, RECT *rc) {
    int n = TabCtrl_GetItemCount(hTab);
    if (idx < 0 || idx >= n) return FALSE;
    RECT r;
    if (!TabCtrl_GetItemRect(hTab, idx, &r)) return FALSE;
    POINT pt = { r.left, r.top };
    ScreenToClient(hTab, &pt);
    SetRect(rc, pt.x, pt.y, pt.x + (r.right - r.left), pt.y + (r.bottom - r.top));
    return TRUE;
}

static RECT close_btn_rect(HWND hTab, int idx) {
    RECT rc, btn;
    SetRectEmpty(&btn);
    if (!get_item_rect(hTab, idx, &rc)) return btn;
    int h = rc.bottom - rc.top;
    int y = rc.top + (h - CLOSE_BTN_SZ) / 2;
    SetRect(&btn, rc.right - CLOSE_BTN_SZ - CLOSE_BTN_PAD, y,
                 rc.right - CLOSE_BTN_PAD, y + CLOSE_BTN_SZ);
    return btn;
}

/* ---------- Owner-draw item tab (TCS_OWNERDRAWFIXED) ---------- */
static void draw_tab_item(LPDRAWITEMSTRUCT dis) {
    HWND hTab = dis->hwndItem;
    int i = (int)dis->itemID;
    HDC hdc = dis->hDC;
    const ThemePalette *pal = theme_palette();

    RECT rc = dis->rcItem;
    BOOL isSelected = (dis->itemState & ODS_SELECTED) != 0;
    BOOL isHover    = (i == g_hoverTab);
    BOOL closeHot   = (i == g_closeHot);

    /* warna latar tab */
    COLORREF bg;
    if (isSelected)       bg = pal->tabSelBg;
    else if (isHover)     bg = pal->tabHotBg;
    else                  bg = pal->tabIdleBg;

    HBRUSH b = CreateSolidBrush(bg);
    FillRect(hdc, &rc, b);
    DeleteObject(b);

    /* garis aksen tipis di sekeliling tab aktif */
    HPEN pen = CreatePen(PS_SOLID, 1, pal->accent);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    if (isSelected) Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);

    /* teks tab */
    WCHAR text[300] = L"";
    TCITEMW tie = {0};
    tie.mask = TCIF_TEXT;
    tie.pszText = text;
    tie.cchTextMax = (int)(sizeof(text) / sizeof(text[0])) - 1;
    TabCtrl_GetItem(hTab, i, &tie);

    HFONT oldFont = (HFONT)SelectObject(hdc, g_hMonoFont);
    RECT trc = rc;
    trc.left += 8;
    trc.right -= CLOSE_BTN_SZ + CLOSE_BTN_PAD + 6;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, pal->text);
    DrawTextW(hdc, text, -1, &trc,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SelectObject(hdc, oldFont);

    /* tombol close: hanya tampil saat tab di-hover */
    int th = rc.bottom - rc.top;
    RECT btn;
    SetRect(&btn, rc.right - CLOSE_BTN_SZ - CLOSE_BTN_PAD,
                 rc.top + (th - CLOSE_BTN_SZ) / 2,
                 rc.right - CLOSE_BTN_PAD,
                 rc.top + (th - CLOSE_BTN_SZ) / 2 + CLOSE_BTN_SZ);
    if (i >= 0 && i < MAX_TABS) {
        g_tabs[i].closeBtn = btn;
        g_tabs[i].closeBtnVisible = isHover;
        g_tabs[i].closeBtnHovered = closeHot;
    }
    if (isHover) {
        COLORREF btnBg = closeHot ? pal->closeHover : pal->closeBg;
        COLORREF btnFg = closeHot ? RGB(255, 255, 255) : pal->text;
        HBRUSH bb = CreateSolidBrush(btnBg);
        FillRect(hdc, &btn, bb);
        DeleteObject(bb);
        SetTextColor(hdc, btnFg);
        DrawTextW(hdc, L"\215", -1, &btn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

/* Background strip tab (area kosong di sekitar tab) ikut warna tema */
static void erase_strip(HWND hTab, HDC hdc) {
    RECT crc;
    GetClientRect(hTab, &crc);
    HBRUSH b = CreateSolidBrush(theme_tab_bg());
    FillRect(hdc, &crc, b);
    DeleteObject(b);
}

/* ---------- Subclass TABWINDOW ---------- */
static LRESULT CALLBACK TabSubProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        int hit = TabCtrl_HitTest(h, &pt);
        int hot = -1;
        if (hit >= 0) {
            RECT btn = close_btn_rect(h, hit);
            if (PtInRect(&btn, pt)) hot = hit;
        }
        if (hit != g_hoverTab || hot != g_closeHot) {
            g_hoverTab = hit;
            g_closeHot = hot;
            InvalidateRect(h, NULL, FALSE);
        }
        break;
    }
    case WM_NCMOUSELEAVE:
    case WM_MOUSELEAVE: {
        if (g_hoverTab != -1 || g_closeHot != -1) {
            g_hoverTab = -1;
            g_closeHot = -1;
            InvalidateRect(h, NULL, FALSE);
        }
        break;
    }
    case WM_SETCURSOR: {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(h, &pt);
        int hit = TabCtrl_HitTest(h, &pt);
        if (hit >= 0) {
            RECT btn = close_btn_rect(h, hit);
            if (PtInRect(&btn, pt)) {
                SetCursor(LoadCursorW(NULL, IDC_ARROW));
                return TRUE;
            }
        }
        break;
    }
    case WM_MBUTTONUP: {
        /* klik-tengah pada tab: langsung tutup (tanpa konfirmasi) */
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        int hit = TabCtrl_HitTest(h, &pt);
        if (hit >= 0) {
            force_close_tab(hit);
            g_hoverTab = -1;
            g_closeHot = -1;
            tabs_notify_switch();
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        break;
    }
    case WM_LBUTTONUP: {
        POINT pt = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        int hit = TabCtrl_HitTest(h, &pt);
        if (hit >= 0) {
            RECT btn = close_btn_rect(h, hit);
            if (PtInRect(&btn, pt)) {
                close_tab(hit);          /* ada konfirmasi bila dirty */
                g_hoverTab = -1;
                g_closeHot = -1;
                InvalidateRect(h, NULL, FALSE);
                return 0;   /* jangan diteruskan: tab tidak jadi dipilih */
            }
        }
        break;
    }
    case WM_DRAWITEM:
        draw_tab_item((LPDRAWITEMSTRUCT)l);
        return TRUE;
    case WM_ERASEBKGND:
        erase_strip(h, (HDC)w);
        return 1;
    }
    return CallWindowProcW(g_origTabProc, h, m, w, l);
}

/* Pasang subclass pada tab control (dipanggil sekali saat WM_CREATE) */
void theme_install_tab_subclass(HWND hTab) {
    if (!hTab || g_origTabProc) return;
    g_origTabProc = (WNDPROC)SetWindowLongPtrW(hTab, GWLP_WNDPROC,
                                               (LONG_PTR)TabSubProc);
}

/* Aplikasikan ulang tema ke seluruh kontrol */
void theme_apply(HWND hMain) {
    ensure_brushes();

    if (g_hTab) {
        SendMessageW(g_hTab, TCM_SETITEMSIZE, 0, MAKELPARAM(160, TABSTRIP_H - 2));
        InvalidateRect(g_hTab, NULL, TRUE);
    }
    if (g_hTree) {
        const ThemePalette *pal = theme_palette();
        TreeView_SetBkColor(g_hTree, pal->winBg);
        TreeView_SetTextColor(g_hTree, pal->text);
        InvalidateRect(g_hTree, NULL, TRUE);
    }

    for (int i = 0; i < g_tabCount; i++)
        if (g_tabs[i].hEdit)
            InvalidateRect(g_tabs[i].hEdit, NULL, TRUE);

    theme_on_settingchange(hMain, 0, 0);
}
