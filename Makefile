.POSIX:

CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

# Detect wlroots and dependencies
PKGS = wlroots-0.20 wayland-server xkbcommon xcb xcb-ewmh xcb-icccm
CFLAGS_PKG != $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null || $(PKG_CONFIG) --cflags wlroots wayland-server xkbcommon xcb xcb-ewmh xcb-icccm
LIBS != $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null || $(PKG_CONFIG) --libs wlroots wayland-server xkbcommon xcb xcb-ewmh xcb-icccm

FUSE_CFLAGS != $(PKG_CONFIG) --cflags fuse3 2>/dev/null
FUSE_LIBS != $(PKG_CONFIG) --libs fuse3 2>/dev/null

ALL_CFLAGS = $(CFLAGS) $(CFLAGS_PKG) -I. -DWLR_USE_UNSTABLE
ALL_LIBS = $(LIBS)

all: mint mintctl mint-keysd mint-fs

release: CFLAGS += -O2 -DNDEBUG
release: LDFLAGS += -s
release: all

wlr-layer-shell-unstable-v1-protocol.h: wlr-layer-shell-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header $< $@

mint.o: mint.c wlr-layer-shell-unstable-v1-protocol.h
	$(CC) -c $< $(ALL_CFLAGS) -o $@

mint: mint.o
	$(CC) $^ $(LDFLAGS) $(ALL_LIBS) -o $@

mintctl: mintctl.c
	$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@

mint-fs: mint-fs.c
	$(CC) $< $(CFLAGS) $(FUSE_CFLAGS) $(LDFLAGS) $(FUSE_LIBS) -o $@

config.h:
	cp config.def.h $@

mint-keysd: mint-keysd.c config.h
	$(CC) $< $(CFLAGS) $(LDFLAGS) -lxkbcommon -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 mint $(DESTDIR)$(BINDIR)/mint
	install -m 755 mintctl $(DESTDIR)$(BINDIR)/mintctl
	install -m 755 mint-keysd $(DESTDIR)$(BINDIR)/mint-keysd
	install -m 755 mint-fs $(DESTDIR)$(BINDIR)/mint-fs

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mint
	rm -f $(DESTDIR)$(BINDIR)/mintctl
	rm -f $(DESTDIR)$(BINDIR)/mint-keysd
	rm -f $(DESTDIR)$(BINDIR)/mint-fs

clean:
	rm -f mint mint.o mintctl mint-keysd mint-fs wlr-layer-shell-unstable-v1-protocol.h

.PHONY: all release install uninstall clean
