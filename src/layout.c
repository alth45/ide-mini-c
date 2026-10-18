/*
 * layout.c  -  Pengaturan posisi/ukuran kontrol (sidebar, splitter, tab, editor)
 */
#include "app.h"

void do_layout(void) {
    if (!g_hMain || !g_hTree || !g_hTab || !g_hSplit) return;

    RECT rc;
    GetClientRect(g_hMain, &rc);
    int W = rc.right, H = rc.bottom;

    if (g_splitX < SIDEBAR_MIN) g_splitX = SIDEBAR_MIN;
    if (g_splitX > W - 150)     g_splitX = W - 150;

    MoveWindow(g_hTree,  0,        0, g_splitX, H, TRUE);
    MoveWindow(g_hSplit, g_splitX, 0, SPLIT_W,  H, TRUE);

    int ex = g_splitX + SPLIT_W;
    int ew = W - ex;

    MoveWindow(g_hTab, ex, 0, ew, TABSTRIP_H, TRUE);
    for (int i = 0; i < g_tabCount; i++)
        MoveWindow(g_tabs[i].hEdit, ex, TABSTRIP_H, ew, H - TABSTRIP_H, TRUE);

    InvalidateRect(g_hTab, NULL, TRUE);
}

void show_tab(int idx) {
    for (int i = 0; i < g_tabCount; i++)
        ShowWindow(g_tabs[i].hEdit, i == idx ? SW_SHOW : SW_HIDE);
    if (idx >= 0 && idx < g_tabCount) SetFocus(g_tabs[idx].hEdit);
    if (g_hTab) InvalidateRect(g_hTab, NULL, TRUE);
}
