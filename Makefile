# Makefile - Mini IDE (Win32, C)
#
# Build dengan MinGW-w64:   mingw32-make          (atau: make CC=x86_64-w64-mingw32-gcc)
# Build dengan MSVC (cl):   nmake /f Makefile.msvc  — atau lihat catatan di bawah.

CC      ?= gcc
RC      ?= windres
EXE     ?= .exe

TARGET  = mini_ide$(EXE)
OBJDIR  = build
SRC     = src/main.c src/util.c src/layout.c src/tabs.c src/fileio.c src/tree.c src/theme.c
OBJ     = $(SRC:src/%.c=$(OBJDIR)/%.o)

CFLAGS  = -Os -Wall -DUNICODE -D_UNICODE -Isrc
LDFLAGS = -municode -mwindows
LIBS    = -lcomctl32 -lshell32 -lole32 -ldwmapi -luxtheme

RES     = mini_ide_win.res

all: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c src/app.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ) $(RES)
	$(CC) $(OBJ) $(RES) $(LDFLAGS) $(LIBS) -o $@

$(RES): mini_ide_win.rc mini_ide_win.manifest
	$(RC) mini_ide_win.rc -O coff -o $(RES)

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
