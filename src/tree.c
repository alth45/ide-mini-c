/*
 * tree.c  -  Sidebar tree view: isi folder, pilih folder
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include "app.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

static BOOL is_ignored_dir(const WCHAR *name) {
    static const WCHAR *skip[] = {
        L".git", L"node_modules", L"build", L"dist", L"__pycache__"
    };
    for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); i++)
        if (wcscmp(name, skip[i]) == 0) return TRUE;
    return FALSE;
}

static void populate_tree(HWND hTree, HTREEITEM parent, const WCHAR *dir) {
    WCHAR pattern[MAX_PATH];
    lstrcpynW(pattern, dir, MAX_PATH);
    if (wcslen(pattern) + 3 < MAX_PATH) wcscat(pattern, L"\\*");

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    /* folder dulu */
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (is_ignored_dir(fd.cFileName)) continue;

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

    /* lalu file */
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

void open_folder(const WCHAR *folder) {
    TreeView_DeleteAllItems(g_hTree);
    free_paths();
    populate_tree(g_hTree, NULL, folder);
}

BOOL pick_folder(WCHAR *out, DWORD size) {
    BROWSEINFOW bi = {0};
    bi.lpszTitle = L"Pilih folder project";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return FALSE;
    BOOL ok = SHGetPathFromIDListW(pidl, out);
    CoTaskMemFree(pidl);
    return ok;
}
