# Mini IDE (Win32 C)

Mini IDE sederhana untuk Windows: sidebar tree folder + editor multi-tab.

## Struktur proyek

```
src/
  app.h      - Header agregat: include tabs.h & theme.h, state global, prototipe
  tabs.h     - Struct Tab, MAX_TABS, API manajemen tab
  theme.h    - Struct ThemePalette (palet warna), API dark mode
  main.c     - wWinMain, jendela utama, menu, accelerator, splitter, WndProc
  layout.c   - do_layout() & show_tab(): posisi sidebar/splitter/tab/editor
  tabs.c     - Manajemen tab: buka/tutup, judul tab, penanda dirty (●)
  fileio.c   - Baca/tulis file teks (UTF-8 + deteksi BOM, fallback ACP)
  tree.c     - Sidebar TreeView: isi folder, dialog pilih folder
  theme.c    - Dark mode + tombol close pada tab saat hover (subclass TABWINDOW)
  util.c     - Utilitas path (track_path, free_paths, path_join)
mini_ide_win.rc / .manifest - resource & manifest Common Controls v6
Makefile                    - build dengan MinGW-w64
```

## Build

### MinGW-w64 (Windows atau cross-compile Linux)
```sh
make                          # pakai gcc/windres default
make CC=x86_64-w64-mingw32-gcc RC=x86_64-w64-mingw32-windres   # cross-compile
```
Menghasilkan `mini_ide.exe`. Library yang dibutuhkan:
`comctl32 shell32 ole32 dwmapi uxtheme`.

### MSVC (Developer Command Prompt)
```bat
rc mini_ide_win.rc
cl /Os /DUNICODE /D_UNICODE /Isrc src\*.c mini_ide_win.res ^
   /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup ^
   comctl32.lib shell32.lib ole32.lib dwmapi.lib uxtheme.lib /OUT:mini_ide.exe
```
(`#pragma comment(lib, ...)` di sumber juga sudah mendeklarasikan lib utk MSVC.)

## Fitur baru

1. **Tombol close per-tab saat hover** (`theme.c`)
   - Tab control memakai `TCS_OWNERDRAWFIXED` + subclass.
   - Saat mouse berada di atas sebuah tab, tanda `×` muncul di sisi kanan tab.
   - Kursor persis di atas `×` mem-highlight kotak menjadi merah (gaya VS Code).
   - Klik `×` menutup tab tersebut (klik di badan tab tetap memilih tab).
   - Bila tab masih *dirty*, ditanyakan dulu sebelum ditutup.
   - Bonus: **klik-tengah** pada tab langsung menutup tanpa konfirmasi.

2. **Dark mode** (`theme.c`, toggle via menu **View → Dark Mode** atau `Ctrl+D`)
   - Palet warna terpusat dalam struct `ThemePalette` (`theme.h`) — satu sumber
     untuk semua warna light/dark (editor, tab, strip, splitter, sidebar).
   - Title bar & menu ikut gelap lewat DWM `immersive dark mode` (Win10 1809+/Win11).
   - Editor, tab strip, splitter, dan TreeView sidebar menyesuaikan warna tema.

3. **Penanda belum tersimpan**: judul tab menampilkan `●` bila ada perubahan
   (deteksi lewat `EN_CHANGE`), hilang setelah Save (`Ctrl+S`).

## Shortcut
| Aksi            | Tombol   |
|-----------------|----------|
| Open Folder     | Ctrl+O   |
| Save            | Ctrl+S   |
| Close Tab       | Ctrl+W   |
| Toggle Dark Mode| Ctrl+D   |
