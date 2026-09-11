# minTwm build configuration
# Copy this file to config.mk and customize your build options.

# ==============================================================================
# Wayland Protocols & Extensions (1 = enable, 0 = disable)
# ==============================================================================

# XWayland support (running legacy X11 applications)
# Disabling this removes dependencies on xcb, xcb-ewmh, and xcb-icccm
XWAYLAND ?= 1

# ext-session-lock-v1 support (swaylock, hyprlock, etc.)
SESSION_LOCK ?= 1

# wlr-layer-shell-unstable-v1 support (status bars like waybar, wallpapers, etc.)
LAYER_SHELL ?= 1

# wlr-screencopy-unstable-v1 support (grim, slurp screen recording and screenshots)
SCREENCOPY ?= 1

# xdg-output-unstable-v1 support (output naming and fractional scaling info)
XDG_OUTPUT ?= 1

# xdg-decoration-unstable-v1 & server-decoration (server-side decoration negotiation)
XDG_DECORATION ?= 1

# wlr-viewporter support (surface cropping and scaling)
VIEWPORTER ?= 1

# wlr-data-control-v1 & ext-data-control-v1 (clipboard managers like wl-clipboard)
DATA_CONTROL ?= 1

# Primary selection protocol (middle-click clipboard paste)
PRIMARY_SELECTION ?= 1

# ==============================================================================
# Window Management Features (1 = enable, 0 = disable)
# ==============================================================================

# Window swallowing (dwm-style swallowing: hide terminal when child GUI app opens)
SWALLOWING ?= 1

# Window gaps and smart gaps support
GAPS ?= 1

# ==============================================================================
# Utilities to Build & Install (1 = enable, 0 = disable)
# ==============================================================================

# mint-fs (FUSE virtual filesystem daemon, requires fuse3)
BUILD_MINT_FS ?= 1

# mint-keysd (Standalone keybinding daemon, requires xkbcommon)
BUILD_MINT_KEYSD ?= 1

# mintctl (Simple IPC command-line client)
BUILD_MINTCTL ?= 1

# ==============================================================================
# Paths & Toolchain
# ==============================================================================
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

CC ?= cc
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
