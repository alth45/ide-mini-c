/*
 * theme.h  -  Palet warna & API dark mode
 */
#ifndef THEME_H
#define THEME_H

#include <windows.h>

typedef struct {
    COLORREF winBg;      /* background jendela utama / strip tab   */
    COLORREF text;       /* teks umum                              */
    COLORREF textDim;    /* teks sekunder                          */
    COLORREF editBg;     /* background editor                      */
    COLORREF editFg;     /* teks editor                            */
    COLORREF tabSelBg;   /* latar tab aktif                        */
    COLORREF tabHotBg;   /* latar tab saat hover                   */
    COLORREF tabIdleBg;  /* latar tab biasa                        */
    COLORREF closeBg;    /* kotak tombol close                     */
    COLORREF closeHover; /* kotak tombol close saat di-hover       */
    COLORREF accent;     /* garis aksen (selection)                */
    COLORREF splitter;   /* garis splitter                         */
} ThemePalette;

extern BOOL g_darkMode;
/* Dipanggil dari WM_DRAWITEM di WndProc parent (owner-drawn tab) */
BOOL theme_handle_drawitem(LPDRAWITEMSTRUCT dis);

const ThemePalette *theme_palette(void);

void     theme_apply(HWND hMain);                  /* refresh tema semua kontrol */
void     theme_install_tab_subclass(HWND hTab);    /* pasang hover/close button  */
HBRUSH   theme_edit_brush(HWND hEdit);             /* brush utk control EDIT     */
LRESULT  theme_on_settingchange(HWND h, WPARAM w, LPARAM l); /* immersive dark   */
COLORREF theme_tab_bg(void);
COLORREF theme_tab_fg(void);
COLORREF theme_edit_fg(void);

#endif /* THEME_H */
