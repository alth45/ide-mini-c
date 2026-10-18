/*
 * util.c  -  Utilitas path & manajemen memori sederhana
 */
#include "app.h"

typedef struct PathNode {
    WCHAR *path;
    struct PathNode *next;
} PathNode;

static PathNode *g_pathList = NULL;

WCHAR *track_path(const WCHAR *p) {
    WCHAR *c = _wcsdup(p);
    PathNode *n = (PathNode *)malloc(sizeof(PathNode));
    n->path = c;
    n->next = g_pathList;
    g_pathList = n;
    return c;
}

void free_paths(void) {
    while (g_pathList) {
        PathNode *n = g_pathList;
        g_pathList = n->next;
        free(n->path);
        free(n);
    }
}

void path_join(WCHAR *out, DWORD outSize, const WCHAR *a, const WCHAR *b) {
    lstrcpynW(out, a, outSize);
    size_t len = wcslen(out);
    if (len + 1 < outSize) {
        out[len++] = L'\\';
        lstrcpynW(out + len, b, outSize - (int)len);
    }
}
