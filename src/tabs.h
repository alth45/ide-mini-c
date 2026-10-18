/*
 * tabs.h  -  Struktur data & API manajemen tab
 */
#ifndef TABS_H
#define TABS_H

#include <windows.h>

#define MAX_TABS 64

typedef struct {
    HWND  hEdit;              /* control EDIT milik tab ini            */
    WCHAR path[MAX_PATH];     /* path file yang sedang dibuka          */
    BOOL  dirty;              /* ada perubahan belum disimpan?         */
    RECT  closeBtn;           /* rect tombol "x" pada tab               */
    BOOL  closeBtnVisible;    /* tombol close terlihat (tab di-hover)?  */
    BOOL  closeBtnHovered;    /* kursor persis di atas tombol close?    */
} Tab;

extern Tab g_tabs[MAX_TABS];
extern int g_tabCount;

int  find_tab(const WCHAR *path);
void open_file_in_tab(const WCHAR *path);
void close_tab(int idx);                 /* dengan konfirmasi bila dirty */
void force_close_tab(int idx);           /* tutup tanpa konfirmasi       */
void update_tab_title(int idx);          /* refresh teks tab (tanda dirty) */
void tabs_check_dirty(HWND hEdit);       /* dipanggil dari EN_CHANGE     */
void tabs_notify_switch(void);           /* notifikasi pergantian tab aktif */

#endif /* TABS_H */
