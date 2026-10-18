/*
 * app.h  -  Header agregat Mini IDE
 *
 * Struktur proyek:
 *   main.c   : entry point, jendela utama, menu, accelerator, splitter
 *   layout.c : posisi/ukuran kontrol (sidebar, splitter, tab strip, editor)
 *   tabs.c   : manajemen tab (buka/tutup, judul, penanda dirty)
 *   fileio.c : baca/tulis file teks (UTF-8 + BOM, fallback ACP)
 *   tree.c   : sidebar TreeView & dialog pilih folder
 *   theme.c  : dark mode + tombol close per-tab saat hover
 *   util.c   : utilitas path
 */
#ifndef APP_H
#define APP_H

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "tabs.h"    /* Tab, g_tabs, MAX_TABS, API tab     */
#include "theme.h"   /* ThemePalette, g_darkMode, API tema */

#define SIDEBAR_MIN  100
#define TABSTRIP_H   26      /* tinggi strip tab / editor area */
#define SPLIT_W      4       /* lebar garis splitter           */

/* Command IDs (menu, accelerator, custom WM_COMMAND) */
enum {
    IDM_OPEN_FOLDER = 1001,
    IDM_SAVE,
    IDM_CLOSE_TAB,
    IDM_EXIT,
    IDM_TOGGLE_DARK,
    IDM_ABOUT             /* Help -> About */
};

/* ---------- Global state (didefinisikan di main.c) ---------- */
extern HWND  g_hMain;     /* jendela utama        */
extern HWND  g_hTree;     /* tree view sidebar    */
extern HWND  g_hTab;      /* tab control          */
extern HWND  g_hSplit;    /* splitter window      */
extern int   g_splitX;    /* posisi splitter      */
extern HFONT g_hMonoFont;
extern HACCEL g_hAccel;
extern BOOL  g_dragging;

/* ---------- Utilitas path (util.c) ---------- */
WCHAR *track_path(const WCHAR *p);      /* salin & kelola memori path   */
void   free_paths(void);                /* bebaskan semua path terlacak */
void   path_join(WCHAR *out, DWORD outSize, const WCHAR *a, const WCHAR *b);

/* ---------- Layout (layout.c) ---------- */
void do_layout(void);
void show_tab(int idx);

/* ---------- File I/O (fileio.c) ---------- */
WCHAR *read_text_file(const WCHAR *path);
BOOL   save_tab(int idx);

/* ---------- Tree/sidebar (tree.c) ---------- */
void open_folder(const WCHAR *folder);
BOOL pick_folder(WCHAR *out, DWORD size);

#endif /* APP_H */
