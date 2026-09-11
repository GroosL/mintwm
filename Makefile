.POSIX:

# Include user config if it exists, otherwise fall back to defaults
-include config.mk
include config.def.mk

PKG_CONFIG ?= pkg-config
WAYLAND_SCANNER ?= wayland-scanner

# Base dependencies
ifeq ($(XWAYLAND),1)
PKGS = wlroots-0.20 wayland-server xkbcommon xcb xcb-ewmh xcb-icccm
PKGS_FALLBACK = wlroots wayland-server xkbcommon xcb xcb-ewmh xcb-icccm
else
PKGS = wlroots-0.20 wayland-server xkbcommon
PKGS_FALLBACK = wlroots wayland-server xkbcommon
endif

CFLAGS_PKG != $(PKG_CONFIG) --cflags $(PKGS) 2>/dev/null || $(PKG_CONFIG) --cflags $(PKGS_FALLBACK)
LIBS != $(PKG_CONFIG) --libs $(PKGS) 2>/dev/null || $(PKG_CONFIG) --libs $(PKGS_FALLBACK)

FUSE_CFLAGS != $(PKG_CONFIG) --cflags fuse3 2>/dev/null
FUSE_LIBS != $(PKG_CONFIG) --libs fuse3 2>/dev/null

# Feature flags passed to compiler
DEFINES = -DWLR_USE_UNSTABLE
DEFINES += -DCONFIG_XWAYLAND=$(XWAYLAND)
DEFINES += -DCONFIG_SESSION_LOCK=$(SESSION_LOCK)
DEFINES += -DCONFIG_LAYER_SHELL=$(LAYER_SHELL)
DEFINES += -DCONFIG_SCREENCOPY=$(SCREENCOPY)
DEFINES += -DCONFIG_XDG_OUTPUT=$(XDG_OUTPUT)
DEFINES += -DCONFIG_XDG_DECORATION=$(XDG_DECORATION)
DEFINES += -DCONFIG_VIEWPORTER=$(VIEWPORTER)
DEFINES += -DCONFIG_DATA_CONTROL=$(DATA_CONTROL)
DEFINES += -DCONFIG_PRIMARY_SELECTION=$(PRIMARY_SELECTION)
DEFINES += -DCONFIG_SWALLOWING=$(SWALLOWING)
DEFINES += -DCONFIG_GAPS=$(GAPS)

ALL_CFLAGS = $(CFLAGS) $(CFLAGS_PKG) $(DEFINES) -I.
ALL_LIBS = $(LIBS)

TARGETS = mint
ifeq ($(BUILD_MINTCTL),1)
TARGETS += mintctl
endif
ifeq ($(BUILD_MINT_KEYSD),1)
TARGETS += mint-keysd
endif
ifeq ($(BUILD_MINT_FS),1)
TARGETS += mint-fs
endif

all: $(TARGETS)

config.mk:
	cp config.def.mk $@

release: CFLAGS += -O2 -DNDEBUG
release: LDFLAGS += -s
release: all

wlr-layer-shell-unstable-v1-protocol.h: wlr-layer-shell-unstable-v1.xml
	$(WAYLAND_SCANNER) server-header $< $@

ifeq ($(LAYER_SHELL),1)
MINT_DEPS = wlr-layer-shell-unstable-v1-protocol.h
else
MINT_DEPS =
endif

mint.o: mint.c $(MINT_DEPS)
	$(CC) -c $< $(ALL_CFLAGS) -o $@

mint: mint.o
	$(CC) $^ $(LDFLAGS) $(ALL_LIBS) -o $@

mintctl: mintctl.c
	$(CC) $< $(CFLAGS) $(LDFLAGS) -o $@

mint-fs: mint-fs.c
	$(CC) $< $(CFLAGS) $(DEFINES) $(FUSE_CFLAGS) $(LDFLAGS) $(FUSE_LIBS) -o $@

config.h:
	cp config.def.h $@

mint-keysd: mint-keysd.c config.h
	$(CC) $< $(CFLAGS) $(LDFLAGS) -lxkbcommon -o $@

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 mint $(DESTDIR)$(BINDIR)/mint
ifeq ($(BUILD_MINTCTL),1)
	install -m 755 mintctl $(DESTDIR)$(BINDIR)/mintctl
endif
ifeq ($(BUILD_MINT_KEYSD),1)
	install -m 755 mint-keysd $(DESTDIR)$(BINDIR)/mint-keysd
endif
ifeq ($(BUILD_MINT_FS),1)
	install -m 755 mint-fs $(DESTDIR)$(BINDIR)/mint-fs
endif

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/mint
	rm -f $(DESTDIR)$(BINDIR)/mintctl
	rm -f $(DESTDIR)$(BINDIR)/mint-keysd
	rm -f $(DESTDIR)$(BINDIR)/mint-fs

clean:
	rm -f mint mint.o mintctl mint-keysd mint-fs wlr-layer-shell-unstable-v1-protocol.h

distclean: clean
	rm -f config.mk config.h

.PHONY: all release install uninstall clean distclean
