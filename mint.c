#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

/* Build configuration defaults (can be overridden via compiler -D flags from config.mk) */
#ifndef CONFIG_XWAYLAND
#define CONFIG_XWAYLAND 1
#endif
#ifndef CONFIG_SESSION_LOCK
#define CONFIG_SESSION_LOCK 1
#endif
#ifndef CONFIG_LAYER_SHELL
#define CONFIG_LAYER_SHELL 1
#endif
#ifndef CONFIG_SCREENCOPY
#define CONFIG_SCREENCOPY 1
#endif
#ifndef CONFIG_XDG_OUTPUT
#define CONFIG_XDG_OUTPUT 1
#endif
#ifndef CONFIG_XDG_DECORATION
#define CONFIG_XDG_DECORATION 1
#endif
#ifndef CONFIG_VIEWPORTER
#define CONFIG_VIEWPORTER 1
#endif
#ifndef CONFIG_DATA_CONTROL
#define CONFIG_DATA_CONTROL 1
#endif
#ifndef CONFIG_PRIMARY_SELECTION
#define CONFIG_PRIMARY_SELECTION 1
#endif
#ifndef CONFIG_SWALLOWING
#define CONFIG_SWALLOWING 1
#endif
#include <assert.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>

#if CONFIG_XDG_DECORATION
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>
#endif

#if CONFIG_LAYER_SHELL
#include <wlr/types/wlr_layer_shell_v1.h>
#endif

#if CONFIG_XDG_OUTPUT
#include <wlr/types/wlr_xdg_output_v1.h>
#endif

#if CONFIG_SCREENCOPY
#include <wlr/types/wlr_screencopy_v1.h>
#endif

#if CONFIG_SESSION_LOCK
#include <wlr/types/wlr_session_lock_v1.h>
#endif

#if CONFIG_DATA_CONTROL
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_ext_data_control_v1.h>
#endif

#if CONFIG_PRIMARY_SELECTION
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#endif

#if CONFIG_VIEWPORTER
#include <wlr/types/wlr_viewporter.h>
#endif

#if CONFIG_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>

#define NUM_WORKSPACES 9
#define DEFAULT_MFACT 0.55
#if CONFIG_GAPS
#define DEFAULT_GAPS 10
#define DEFAULT_SMART_GAPS true
#endif

struct mint_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	/* Scene trees for stacking order */
	struct wlr_scene_tree *scene_tree_background;
	struct wlr_scene_tree *scene_tree_bottom;
	struct wlr_scene_tree *scene_tree_windows;
	struct wlr_scene_tree *scene_tree_top;
	struct wlr_scene_tree *scene_tree_fullscreen;
	struct wlr_scene_tree *scene_tree_overlay;
#if CONFIG_SESSION_LOCK
	struct wlr_scene_tree *scene_tree_lock;
	struct wlr_scene_rect *lock_bg;
#endif

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;

#if CONFIG_XDG_DECORATION
	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
	struct wl_listener new_xdg_decoration;
	struct wlr_server_decoration_manager *server_decoration_mgr;
#endif

	struct wlr_compositor *compositor;
#if CONFIG_XWAYLAND
	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_ready;
	struct wl_listener new_xwayland_surface;
#endif

#if CONFIG_LAYER_SHELL
	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener new_layer_shell_surface;
#endif

#if CONFIG_SCREENCOPY
	struct wlr_screencopy_manager_v1 *screencopy_mgr;
#endif

#if CONFIG_SESSION_LOCK
	struct wlr_session_lock_manager_v1 *session_lock_mgr;
	struct wl_listener new_session_lock;
	struct wlr_session_lock_v1 *session_lock;
	struct wl_listener session_lock_new_surface;
	struct wl_listener session_lock_unlock;
	struct wl_listener session_lock_destroy;
	struct wl_list lock_surfaces;
#endif
	bool locked;

	struct wl_list toplevels;
	struct mint_toplevel *focused_toplevel;
	unsigned int current_workspace;
	unsigned int prev_workspace;
	struct mint_toplevel *last_focused_per_workspace[NUM_WORKSPACES];
	double mfact;
#if CONFIG_GAPS
	int gaps;
	bool gaps_enabled;
	bool smart_gaps;
#endif
#if CONFIG_SWALLOWING
	bool auto_swallow;
#endif

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener pointer_focus_change;
	struct wl_listener request_set_selection;
#if CONFIG_PRIMARY_SELECTION
	struct wl_listener request_set_primary_selection;
#endif
	struct wl_list keyboards;

	struct wlr_output_layout *output_layout;
#if CONFIG_XDG_OUTPUT
	struct wlr_xdg_output_manager_v1 *xdg_output_manager;
#endif
	struct wl_list outputs;
	struct wl_listener new_output;

	/* IPC */
	int ipc_fd;
	struct wl_event_source *ipc_event_source;
	char ipc_socket_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	struct wl_list ipc_clients;
	struct mint_ipc_client *key_grabber;
};

struct mint_output {
	struct wl_list link;
	struct mint_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;

	struct wl_list layers;
	struct wlr_box usable_area;
};

#if CONFIG_SESSION_LOCK
struct mint_session_lock_surface {
	struct wl_list link;
	struct mint_server *server;
	struct mint_output *output;
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener destroy;
};
#endif

enum mint_toplevel_type {
	MINT_TOPLEVEL_XDG,
#if CONFIG_XWAYLAND
	MINT_TOPLEVEL_XWAYLAND,
	MINT_TOPLEVEL_XWAYLAND_UNMANAGED,
#endif
};

struct mint_toplevel {
	struct wl_list link;
	struct mint_server *server;
	enum mint_toplevel_type type;

	struct wlr_xdg_toplevel *xdg_toplevel;
#if CONFIG_XWAYLAND
	struct wlr_xwayland_surface *xwayland_surface;
#endif
	struct wlr_scene_tree *scene_tree;

	bool mapped;
	bool is_fullscreen;
#if CONFIG_SWALLOWING
	bool swallowee_was_fullscreen;
#endif
	unsigned int workspace;
	int pending_x, pending_y;
	int pending_width, pending_height;

#if CONFIG_SWALLOWING
	struct mint_toplevel *swallowed_by;
	struct mint_toplevel *swallowing;
	struct mint_toplevel *last_swallowed;
#endif

#if CONFIG_XDG_DECORATION
	struct wlr_xdg_toplevel_decoration_v1 *decoration;
	struct wl_listener decoration_request_mode;
	struct wl_listener decoration_destroy;
#endif

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
#if CONFIG_XWAYLAND
	struct wl_listener request_configure;
	struct wl_listener request_activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
#endif
	struct wl_listener set_title;
#if CONFIG_XWAYLAND
	struct wl_listener set_override_redirect;
	struct wl_listener set_geometry;
#endif
};

static inline bool toplevel_is_unmanaged(const struct mint_toplevel *tl) {
#if CONFIG_XWAYLAND
	if (!tl) return false;
	if (tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) return true;
	if (tl->type == MINT_TOPLEVEL_XWAYLAND && tl->xwayland_surface && tl->xwayland_surface->override_redirect) return true;
#endif
	return false;
}

#if CONFIG_LAYER_SHELL
struct mint_layer_surface {
	struct wl_list link;
	struct mint_server *server;
	struct mint_output *output;
	struct wlr_layer_surface_v1 *layer_surface;
	struct wlr_scene_layer_surface_v1 *scene_layer_surface;

	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener destroy;
};
#endif

struct mint_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct mint_keyboard {
	struct wl_list link;
	struct mint_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

struct mint_ipc_client {
	struct wl_list link;
	struct mint_server *server;
	int fd;
	struct wl_event_source *event_source;
	bool subscribed;
	char buffer[512];
	size_t len;
};

/* Forward declarations */
static void arrange_windows(struct mint_server *server);
static void arrange_layers(struct mint_output *output);
static void update_lock_bg(struct mint_server *server);
static void focus_toplevel(struct mint_server *server, struct mint_toplevel *toplevel);
static void toplevel_set_fullscreen(struct mint_toplevel *tl, bool fullscreen);
static struct mint_output *get_active_output(struct mint_server *server);
static void ipc_broadcast_state(struct mint_server *server);
static void ipc_execute_command(struct mint_server *server,
		struct mint_ipc_client *client,
		const char *cmd, char *resp, size_t resp_size);
#if CONFIG_SWALLOWING
static void swallow_toplevel(struct mint_server *server, struct mint_toplevel *swallower, struct mint_toplevel *swallowee);
static void unswallow_toplevel(struct mint_server *server, struct mint_toplevel *swallower);
static void try_auto_swallow(struct mint_server *server, struct mint_toplevel *swallower);
static bool toplevel_toggle_swallow(struct mint_server *server, struct mint_toplevel *tl);
#else
static inline void unswallow_toplevel(struct mint_server *server, struct mint_toplevel *swallower) {
	(void)server; (void)swallower;
}
static inline void try_auto_swallow(struct mint_server *server, struct mint_toplevel *swallower) {
	(void)server; (void)swallower;
}
#endif

#if CONFIG_LAYER_SHELL
static struct wlr_scene_tree *get_layer_tree(struct mint_server *server,
		enum zwlr_layer_shell_v1_layer layer) {
	switch (layer) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return server->scene_tree_background;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return server->scene_tree_bottom;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return server->scene_tree_top;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return server->scene_tree_overlay;
	}
	return server->scene_tree_top;
}
#endif

static struct mint_output *get_active_output(struct mint_server *server) {
	if (wl_list_empty(&server->outputs)) {
		return NULL;
	}
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
		server->output_layout, server->cursor->x, server->cursor->y);
	if (wlr_output != NULL && wlr_output->data != NULL) {
		return wlr_output->data;
	}
	struct mint_output *first_output;
	return wl_container_of(server->outputs.next, first_output, link);
}

static struct wlr_surface *toplevel_get_surface(struct mint_toplevel *tl) {
	if (!tl) return NULL;
#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		return tl->xwayland_surface ? tl->xwayland_surface->surface : NULL;
	}
#endif
	return (tl->xdg_toplevel && tl->xdg_toplevel->base) ? tl->xdg_toplevel->base->surface : NULL;
}

static inline bool toplevel_is_mapped(struct mint_toplevel *tl) {
#if CONFIG_SWALLOWING
	return tl != NULL && tl->mapped && tl->swallowed_by == NULL;
#else
	return tl != NULL && tl->mapped;
#endif
}

static const char *toplevel_get_title(struct mint_toplevel *tl) {
	if (!tl) return "";
#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		if (tl->xwayland_surface && tl->xwayland_surface->title && tl->xwayland_surface->title[0] != '\0') {
			return tl->xwayland_surface->title;
		}
		if (tl->xwayland_surface && tl->xwayland_surface->class && tl->xwayland_surface->class[0] != '\0') {
			return tl->xwayland_surface->class;
		}
		return "";
	}
#endif
	if (tl->xdg_toplevel) {
		if (tl->xdg_toplevel->title && tl->xdg_toplevel->title[0] != '\0') {
			return tl->xdg_toplevel->title;
		}
		if (tl->xdg_toplevel->app_id) {
			return tl->xdg_toplevel->app_id;
		}
	}
	return "";
}

static const char *toplevel_get_app_id(struct mint_toplevel *tl) {
	if (!tl) return "";
	if (tl->type == MINT_TOPLEVEL_XDG && tl->xdg_toplevel && tl->xdg_toplevel->app_id) {
		return tl->xdg_toplevel->app_id;
	}
#if CONFIG_XWAYLAND
	if ((tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) && tl->xwayland_surface) {
		if (tl->xwayland_surface->class && tl->xwayland_surface->class[0] != '\0') {
			return tl->xwayland_surface->class;
		}
		if (tl->xwayland_surface->instance && tl->xwayland_surface->instance[0] != '\0') {
			return tl->xwayland_surface->instance;
		}
	}
#endif
	return "";
}

static pid_t toplevel_get_pid(struct mint_toplevel *tl) {
	if (!tl) return 0;
	if (tl->type == MINT_TOPLEVEL_XDG && tl->xdg_toplevel && tl->xdg_toplevel->base &&
	    tl->xdg_toplevel->base->client && tl->xdg_toplevel->base->client->client) {
		pid_t pid = 0;
		wl_client_get_credentials(tl->xdg_toplevel->base->client->client, &pid, NULL, NULL);
		return pid;
	}
#if CONFIG_XWAYLAND
	if ((tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) && tl->xwayland_surface) {
		return tl->xwayland_surface->pid;
	}
#endif
	return 0;
}

#if CONFIG_SWALLOWING
static pid_t get_parent_pid(pid_t pid) {
	if (pid <= 1) return 0;
	char path[64];
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f) return 0;
	char buf[512];
	if (!fgets(buf, sizeof(buf), f)) {
		fclose(f);
		return 0;
	}
	fclose(f);
	char *rparen = strrchr(buf, ')');
	if (!rparen) return 0;
	char state;
	int ppid = 0;
	if (sscanf(rparen + 1, " %c %d", &state, &ppid) == 2) {
		return (pid_t)ppid;
	}
	return 0;
}

static size_t get_ancestor_pids(pid_t pid, pid_t *pids, size_t max_pids) {
	size_t count = 0;
	pid_t cur = pid;
	while (count < max_pids && cur > 1) {
		cur = get_parent_pid(cur);
		if (cur <= 1) break;
		pids[count++] = cur;
	}
	return count;
}

static bool has_ancestor_pid(pid_t target, const pid_t *ancestors, size_t n_ancestors) {
	if (target <= 1) return false;
	for (size_t i = 0; i < n_ancestors; i++) {
		if (ancestors[i] == target) return true;
	}
	return false;
}

static bool is_terminal_app(const char *app_id) {
	if (!app_id || app_id[0] == '\0') return false;
	static const char *terminals[] = {
		"foot",
		"footclient",
		"kitty",
		"alacritty",
		"Alacritty",
		"wezterm",
		"org.wezfurlong.wezterm",
		"xterm",
		"XTerm",
		"urxvt",
		"URxvt",
		"st",
		"st-256color",
		"ghostty",
		"com.mitchellh.ghostty",
		"rio",
		"contour",
		"blackbox",
		"terminator",
		"konsole",
		"gnome-terminal",
		"xfce4-terminal",
		"lxterminal",
	};
	for (size_t i = 0; i < sizeof(terminals) / sizeof(terminals[0]); i++) {
		if (strcasecmp(app_id, terminals[i]) == 0) {
			return true;
		}
	}
	return false;
}
#endif

static void toplevel_set_size_and_position(struct mint_toplevel *tl, int x, int y, int width, int height) {
	tl->pending_x = x;
	tl->pending_y = y;
	tl->pending_width = width;
	tl->pending_height = height;

#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		if (tl->scene_tree) {
			wlr_scene_node_set_position(&tl->scene_tree->node, x, y);
			struct wlr_box clip = { .x = 0, .y = 0, .width = width, .height = height };
			wlr_scene_subsurface_tree_set_clip(&tl->scene_tree->node, &clip);
		}
		if (tl->xwayland_surface) {
			wlr_xwayland_surface_configure(tl->xwayland_surface, x, y, width, height);
		}
	} else
#endif
	{
		if (tl->scene_tree && tl->xdg_toplevel && tl->xdg_toplevel->base) {
			wlr_scene_node_set_position(&tl->scene_tree->node,
				x - tl->xdg_toplevel->base->geometry.x,
				y - tl->xdg_toplevel->base->geometry.y);
		}
		if (tl->xdg_toplevel) {
			wlr_xdg_toplevel_set_size(tl->xdg_toplevel, width, height);
		}
	}
}

static void toplevel_set_activated(struct mint_toplevel *tl, bool activated) {
	if (!tl) return;
#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND) {
		if (tl->xwayland_surface && !tl->xwayland_surface->override_redirect) {
			wlr_xwayland_surface_activate(tl->xwayland_surface, activated);
			if (activated) {
				wlr_xwayland_surface_restack(tl->xwayland_surface, NULL, XCB_STACK_MODE_ABOVE);
			}
		}
	} else if (tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		/* Unmanaged / override-redirect surfaces manage their own activation and stacking */
	} else
#endif
	{
		if (tl->xdg_toplevel) {
			wlr_xdg_toplevel_set_activated(tl->xdg_toplevel, activated);
		}
	}
}

static void toplevel_close(struct mint_toplevel *tl) {
	if (!tl) return;
#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		if (tl->xwayland_surface) {
			wlr_xwayland_surface_close(tl->xwayland_surface);
		}
	} else
#endif
	{
		if (tl->xdg_toplevel) {
			wlr_xdg_toplevel_send_close(tl->xdg_toplevel);
		}
	}
}

static void toplevel_set_fullscreen(struct mint_toplevel *tl, bool fullscreen) {
	if (!tl || tl->is_fullscreen == fullscreen) {
		return;
	}

	tl->is_fullscreen = fullscreen;

	if (tl->scene_tree) {
		if (fullscreen) {
			wlr_scene_node_reparent(&tl->scene_tree->node, tl->server->scene_tree_fullscreen);
			wlr_scene_node_raise_to_top(&tl->scene_tree->node);
		} else {
			wlr_scene_node_reparent(&tl->scene_tree->node, tl->server->scene_tree_windows);
		}
	}

#if CONFIG_XWAYLAND
	if (tl->type == MINT_TOPLEVEL_XWAYLAND || tl->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		if (tl->xwayland_surface) {
			wlr_xwayland_surface_set_fullscreen(tl->xwayland_surface, fullscreen);
		}
	} else
#endif
	{
		if (tl->xdg_toplevel) {
			wlr_xdg_toplevel_set_fullscreen(tl->xdg_toplevel, fullscreen);
			wlr_xdg_toplevel_set_tiled(tl->xdg_toplevel, fullscreen ? 0 :
				(WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT));
		}
	}

	arrange_windows(tl->server);
}

static void arrange_windows(struct mint_server *server) {
	struct mint_output *output = get_active_output(server);
	if (!output) {
		return;
	}

	struct wlr_box full_area = {0};
	wlr_output_layout_get_box(server->output_layout, output->wlr_output, &full_area);

	struct wlr_box area = output->usable_area;
	if (area.width <= 0 || area.height <= 0) {
		area = full_area;
	}

	struct mint_toplevel *tiled[64];
	int count = 0;
	struct mint_toplevel *tl;
	wl_list_for_each(tl, &server->toplevels, link) {
		if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
			if (tl->is_fullscreen) {
				toplevel_set_size_and_position(tl, full_area.x, full_area.y, full_area.width, full_area.height);
			} else if (count < 64) {
				tiled[count++] = tl;
			}
		}
	}

	if (count == 0) {
		return;
	}

#if CONFIG_GAPS
	/* Determine effective gap size */
	int g = (server->gaps_enabled && server->gaps > 0) ? server->gaps : 0;
	if (g > 0 && server->smart_gaps && count == 1) {
		g = 0;
	}

	/* Safety check: ensure gaps don't consume more than available space */
	if (g > 0) {
		if (area.width < 2 * g + 120 || area.height < 2 * g + 120) {
			g = 0;
		}
	}

	if (count == 1) {
		if (g > 0) {
			toplevel_set_size_and_position(tiled[0],
				area.x + g, area.y + g,
				area.width - 2 * g, area.height - 2 * g);
		} else {
			toplevel_set_size_and_position(tiled[0], area.x, area.y, area.width, area.height);
		}
		return;
	}
#else
	int g = 0;
	if (count == 1) {
		toplevel_set_size_and_position(tiled[0], area.x, area.y, area.width, area.height);
		return;
	}
#endif

	/* DWM Master-and-Stack Tiling with Gaps */
	if (g == 0) {
		int mw = (int)(area.width * server->mfact);
		if (mw < 60) mw = 60;
		if (mw > area.width - 60) mw = area.width - 60;

		int sw = area.width - mw;
		int sx = area.x + mw;
		int num_stack = count - 1;

		toplevel_set_size_and_position(tiled[0], area.x, area.y, mw, area.height);
		for (int i = 1; i < count; i++) {
			int sh = area.height / num_stack;
			int si = i - 1;
			int sy = area.y + si * sh;
			int h = (si == num_stack - 1) ? (area.height - si * sh) : sh;

			toplevel_set_size_and_position(tiled[i], sx, sy, sw, h);
		}
	} else {
		int x0 = area.x + g;
		int y0 = area.y + g;
		int inner_w = area.width - 2 * g;
		int inner_h = area.height - 2 * g;

		int avail_w = inner_w - g;
		int mw = (int)(avail_w * server->mfact);
		if (mw < 60) mw = 60;
		if (mw > avail_w - 60) mw = avail_w - 60;

		int sw = avail_w - mw;
		int sx = x0 + mw + g;
		int num_stack = count - 1;

		toplevel_set_size_and_position(tiled[0], x0, y0, mw, inner_h);

		int avail_sh = inner_h - (num_stack - 1) * g;
		int base_sh = (avail_sh > 0) ? (avail_sh / num_stack) : 20;

		for (int i = 1; i < count; i++) {
			int si = i - 1;
			int sy = y0 + si * (base_sh + g);
			int h = (si == num_stack - 1) ? (y0 + inner_h - sy) : base_sh;
			if (h < 20) h = 20;

			toplevel_set_size_and_position(tiled[i], sx, sy, sw, h);
		}
	}
}

static void arrange_layers(struct mint_output *output) {
	if (!output || !output->wlr_output) {
		return;
	}
	struct mint_server *server = output->server;

	struct wlr_box full_area = {0};
	wlr_output_layout_get_box(server->output_layout, output->wlr_output, &full_area);
	struct wlr_box usable_area = full_area;

#if CONFIG_LAYER_SHELL
	static const enum zwlr_layer_shell_v1_layer layers[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
		ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM,
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND,
	};

	for (size_t i = 0; i < sizeof(layers) / sizeof(layers[0]); i++) {
		struct mint_layer_surface *ls;
		wl_list_for_each(ls, &output->layers, link) {
			if (ls->layer_surface->current.layer == layers[i]) {
				wlr_scene_layer_surface_v1_configure(ls->scene_layer_surface, &full_area, &usable_area);
			}
		}
	}
#endif

	struct wlr_box old_area = output->usable_area;
	output->usable_area = usable_area;
	if (memcmp(&old_area, &usable_area, sizeof(struct wlr_box)) != 0) {
		arrange_windows(server);
	}
}

static void focus_toplevel(struct mint_server *server, struct mint_toplevel *toplevel) {
	if (server->locked) {
		return;
	}
#if CONFIG_XWAYLAND
	if (toplevel && toplevel_is_unmanaged(toplevel)) {
		return;
	}
#endif
#if CONFIG_SWALLOWING
	if (toplevel && toplevel->swallowed_by != NULL) {
		toplevel = toplevel->swallowed_by;
	}
#endif
	struct wlr_seat *seat = server->seat;

	if (server->focused_toplevel && server->focused_toplevel != toplevel) {
		toplevel_set_activated(server->focused_toplevel, false);
	}

	if (toplevel == NULL) {
		server->focused_toplevel = NULL;
		wlr_seat_keyboard_clear_focus(seat);
		ipc_broadcast_state(server);
		return;
	}

	server->focused_toplevel = toplevel;
	toplevel_set_activated(toplevel, true);

	if (toplevel->is_fullscreen && toplevel->scene_tree) {
		wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
	}

	struct wlr_surface *surface = toplevel_get_surface(toplevel);
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
	if (surface != NULL && keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(seat, surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	}
	ipc_broadcast_state(server);
}

#if CONFIG_SWALLOWING
static void swallow_toplevel(struct mint_server *server, struct mint_toplevel *swallower, struct mint_toplevel *swallowee) {
	if (!swallower || !swallowee || swallower == swallowee) return;

	if (swallower->swallowing != NULL) {
		unswallow_toplevel(server, swallower);
	}
	if (swallowee->swallowed_by != NULL) {
		unswallow_toplevel(server, swallowee->swallowed_by);
	}
	if (swallowee->swallowing != NULL) {
		unswallow_toplevel(server, swallowee);
	}

	swallower->swallowing = swallowee;
	swallowee->swallowed_by = swallower;
	swallower->last_swallowed = swallowee;
	swallower->swallowee_was_fullscreen = swallowee->is_fullscreen;
	swallowee->workspace = swallower->workspace;

	if (swallowee->is_fullscreen) {
		toplevel_set_fullscreen(swallowee, false);
		toplevel_set_fullscreen(swallower, true);
	}

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == swallowee) {
			server->last_focused_per_workspace[i] = swallower;
		}
	}

	if (swallowee->scene_tree) {
		wlr_scene_node_set_enabled(&swallowee->scene_tree->node, false);
	}

	/* Swallower replaces swallowee in server->toplevels */
	if (!wl_list_empty(&swallower->link)) {
		wl_list_remove(&swallower->link);
	}
	if (!wl_list_empty(&swallowee->link)) {
		wl_list_insert(swallowee->link.prev, &swallower->link);
		wl_list_remove(&swallowee->link);
		wl_list_init(&swallowee->link);
	} else {
		wl_list_insert(&server->toplevels, &swallower->link);
	}

	arrange_windows(server);
	focus_toplevel(server, swallower);
}

static void unswallow_toplevel(struct mint_server *server, struct mint_toplevel *swallower) {
	if (!swallower || !swallower->swallowing) return;
	struct mint_toplevel *swallowee = swallower->swallowing;

	swallower->swallowing = NULL;
	swallowee->swallowed_by = NULL;
	swallower->last_swallowed = swallowee;
	swallowee->workspace = swallower->workspace;

	if (wl_list_empty(&swallowee->link)) {
		if (!wl_list_empty(&swallower->link)) {
			wl_list_insert(swallower->link.prev, &swallowee->link);
		} else {
			wl_list_insert(&server->toplevels, &swallowee->link);
		}
	}

	if (swallower->swallowee_was_fullscreen) {
		toplevel_set_fullscreen(swallowee, true);
	}

	if (swallowee->scene_tree) {
		bool visible = (swallowee->workspace == server->current_workspace);
		wlr_scene_node_set_enabled(&swallowee->scene_tree->node, visible);
	}

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == swallower) {
			server->last_focused_per_workspace[i] = swallowee;
		}
	}

	arrange_windows(server);
}

static void try_auto_swallow(struct mint_server *server, struct mint_toplevel *swallower) {
	if (!server->auto_swallow || !swallower) return;
#if CONFIG_XWAYLAND
	if (swallower->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) return;
#endif

	const char *app_id = toplevel_get_app_id(swallower);
	if (is_terminal_app(app_id)) {
		return;
	}

	pid_t swallower_pid = toplevel_get_pid(swallower);
	if (swallower_pid <= 1) return;

	pid_t ancestors[32];
	size_t n_ancestors = get_ancestor_pids(swallower_pid, ancestors, 32);
	if (n_ancestors == 0) return;

	struct mint_toplevel *target = NULL;

	/* 1. First check currently focused toplevel if it is a terminal on the same workspace */
	struct mint_toplevel *focused = server->focused_toplevel;
	if (focused && focused != swallower && focused->mapped && focused->swallowed_by == NULL &&
	    focused->workspace == swallower->workspace &&
	    is_terminal_app(toplevel_get_app_id(focused))) {
		pid_t term_pid = toplevel_get_pid(focused);
		if (has_ancestor_pid(term_pid, ancestors, n_ancestors)) {
			target = focused;
		}
	}

	/* 2. Check closest ancestor among mapped terminals on the same workspace */
	if (!target) {
		for (size_t i = 0; i < n_ancestors && !target; i++) {
			struct mint_toplevel *tl;
			wl_list_for_each(tl, &server->toplevels, link) {
				if (tl != swallower && tl->mapped && tl->swallowed_by == NULL &&
				    tl->workspace == swallower->workspace &&
				    is_terminal_app(toplevel_get_app_id(tl))) {
					if (ancestors[i] == toplevel_get_pid(tl)) {
						target = tl;
						break;
					}
				}
			}
		}
	}

	if (target) {
		swallow_toplevel(server, swallower, target);
	}
}

static bool toplevel_toggle_swallow(struct mint_server *server, struct mint_toplevel *tl) {
	if (!tl) return false;

	/* If tl is currently swallowing another window: unswallow */
	if (tl->swallowing != NULL) {
		unswallow_toplevel(server, tl);
		return false;
	}

	/* If tl is NOT swallowing anything: find candidate to swallow */
	struct mint_toplevel *target = NULL;

	/* Candidate 1: last_swallowed if still valid */
	if (tl->last_swallowed != NULL &&
	    tl->last_swallowed->mapped &&
	    tl->last_swallowed->swallowed_by == NULL &&
	    tl->last_swallowed->workspace == tl->workspace) {
		target = tl->last_swallowed;
	}

	/* Candidate 2: If tl is not a terminal, look for a terminal ancestor on this workspace */
	if (!target && !is_terminal_app(toplevel_get_app_id(tl))) {
		pid_t tl_pid = toplevel_get_pid(tl);
		pid_t ancestors[32];
		size_t n_ancestors = get_ancestor_pids(tl_pid, ancestors, 32);
		for (size_t i = 0; i < n_ancestors && !target; i++) {
			struct mint_toplevel *item;
			wl_list_for_each(item, &server->toplevels, link) {
				if (item != tl && item->mapped && item->swallowed_by == NULL &&
				    item->workspace == tl->workspace &&
				    is_terminal_app(toplevel_get_app_id(item))) {
					if (ancestors[i] == toplevel_get_pid(item)) {
						target = item;
						break;
					}
				}
			}
		}
	}

	/* Candidate 3: If tl is not a terminal, find any mapped terminal on the workspace */
	if (!target && !is_terminal_app(toplevel_get_app_id(tl))) {
		struct mint_toplevel *item;
		wl_list_for_each(item, &server->toplevels, link) {
			if (item != tl && item->mapped && item->swallowed_by == NULL &&
			    item->workspace == tl->workspace &&
			    is_terminal_app(toplevel_get_app_id(item))) {
				target = item;
				break;
			}
		}
	}

	/* Candidate 4: If tl IS a terminal, find a GUI app on the workspace to swallow it */
	if (!target && is_terminal_app(toplevel_get_app_id(tl))) {
		pid_t tl_pid = toplevel_get_pid(tl);
		struct mint_toplevel *swallower_candidate = NULL;
		struct mint_toplevel *item;
		wl_list_for_each(item, &server->toplevels, link) {
			if (item != tl && item->mapped && item->swallowed_by == NULL &&
			    item->workspace == tl->workspace && item->swallowing == NULL &&
			    !is_terminal_app(toplevel_get_app_id(item))) {
				if (item->last_swallowed == tl) {
					swallower_candidate = item;
					break;
				}
				pid_t item_ancestors[32];
				size_t n_item_anc = get_ancestor_pids(toplevel_get_pid(item), item_ancestors, 32);
				if (has_ancestor_pid(tl_pid, item_ancestors, n_item_anc)) {
					swallower_candidate = item;
					break;
				}
				if (!swallower_candidate) {
					swallower_candidate = item;
				}
			}
		}
		if (swallower_candidate) {
			swallow_toplevel(server, swallower_candidate, tl);
			return true;
		}
	}

	if (target) {
		swallow_toplevel(server, tl, target);
		return true;
	}

	return false;
}

#endif

static void focus_cycle(struct mint_server *server, bool prev) {
	if (server->locked || wl_list_empty(&server->toplevels)) {
		return;
	}

	struct mint_toplevel *current = server->focused_toplevel;
	struct mint_toplevel *first = NULL;
	struct mint_toplevel *last = NULL;
	struct mint_toplevel *target = NULL;
	int count = 0;

	struct mint_toplevel *tl;
	wl_list_for_each(tl, &server->toplevels, link) {
		if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
			count++;
			if (!first) first = tl;
			last = tl;
		}
	}

	if (count <= 1) {
		return;
	}

	if (!current || current->workspace != server->current_workspace) {
		focus_toplevel(server, first);
		return;
	}

	if (!prev) {
		bool found = false;
		wl_list_for_each(tl, &server->toplevels, link) {
			if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
				if (found) {
					target = tl;
					break;
				}
				if (tl == current) {
					found = true;
				}
			}
		}
		if (!target) {
			target = first;
		}
	} else {
		struct mint_toplevel *prev_tl = NULL;
		wl_list_for_each(tl, &server->toplevels, link) {
			if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
				if (tl == current) {
					target = prev_tl;
					break;
				}
				prev_tl = tl;
			}
		}
		if (!target) {
			target = last;
		}
	}

	if (target != NULL) {
		focus_toplevel(server, target);
	}
}

static void focus_master(struct mint_server *server) {
	struct mint_toplevel *tl;
	wl_list_for_each(tl, &server->toplevels, link) {
		if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
			focus_toplevel(server, tl);
			return;
		}
	}
}

static void swap_master(struct mint_server *server) {
	if (wl_list_empty(&server->toplevels)) {
		return;
	}
	struct mint_toplevel *current = server->focused_toplevel;
	if (!current || current->workspace != server->current_workspace) {
		return;
	}

	struct mint_toplevel *first = NULL;
	struct mint_toplevel *tl;
	wl_list_for_each(tl, &server->toplevels, link) {
		if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
			first = tl;
			break;
		}
	}

	if (!first) {
		return;
	}

	if (first == current) {
		/* If current is already master, swap with the next window on workspace */
		struct mint_toplevel *second = NULL;
		bool found_first = false;
		wl_list_for_each(tl, &server->toplevels, link) {
			if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
				if (!found_first) {
					found_first = true;
				} else {
					second = tl;
					break;
				}
			}
		}
		if (second) {
			wl_list_remove(&second->link);
			wl_list_insert(first->link.prev, &second->link);
		}
	} else {
		/* Move current to head of current workspace (master position) */
		wl_list_remove(&current->link);
		wl_list_insert(first->link.prev, &current->link);
		focus_toplevel(server, current);
	}
	arrange_windows(server);
}

static void change_workspace(struct mint_server *server, unsigned int ws) {
	if (ws < 1 || ws > NUM_WORKSPACES || ws == server->current_workspace) {
		return;
	}

	server->last_focused_per_workspace[server->current_workspace - 1] = server->focused_toplevel;
	server->prev_workspace = server->current_workspace;
	server->current_workspace = ws;

	struct mint_toplevel *tl;
	wl_list_for_each(tl, &server->toplevels, link) {
		bool visible = (tl->workspace == ws && toplevel_is_mapped(tl));
		if (tl->scene_tree) {
			wlr_scene_node_set_enabled(&tl->scene_tree->node, visible);
		}
	}

	arrange_windows(server);

	struct mint_toplevel *focus_target = server->last_focused_per_workspace[ws - 1];
	if (!focus_target || !toplevel_is_mapped(focus_target) || focus_target->workspace != ws) {
		focus_target = NULL;
		wl_list_for_each(tl, &server->toplevels, link) {
			if (toplevel_is_mapped(tl) && tl->workspace == ws) {
				focus_target = tl;
				break;
			}
		}
	}
	focus_toplevel(server, focus_target);
}

static void move_to_workspace(struct mint_server *server, unsigned int ws) {
	if (ws < 1 || ws > NUM_WORKSPACES) {
		return;
	}
	struct mint_toplevel *tl = server->focused_toplevel;
	if (!tl || tl->workspace == ws) {
		return;
	}

	tl->workspace = ws;
#if CONFIG_SWALLOWING
	if (tl->swallowing) {
		tl->swallowing->workspace = ws;
	}
#endif
	if (tl->scene_tree) {
		wlr_scene_node_set_enabled(&tl->scene_tree->node, false);
	}

	/* Focus another window on current workspace */
	struct mint_toplevel *next_focus = NULL;
	struct mint_toplevel *item;
	wl_list_for_each(item, &server->toplevels, link) {
		if (item != tl && toplevel_is_mapped(item) && item->workspace == server->current_workspace) {
			next_focus = item;
			break;
		}
	}
	focus_toplevel(server, next_focus);

	arrange_windows(server);
}

/* Keyboard handlers */
static void keyboard_handle_modifiers(struct wl_listener *listener, void *data) {
	struct mint_keyboard *keyboard =
		wl_container_of(listener, keyboard, modifiers);
	wlr_seat_set_keyboard(keyboard->server->seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_modifiers(keyboard->server->seat,
		&keyboard->wlr_keyboard->modifiers);
}

static ssize_t read_reply_line(int fd, char *buf, size_t max, int timeout_ms) {
	if (max == 0) return -1;
	struct pollfd pfd = { .fd = fd, .events = POLLIN };
	int remaining = timeout_ms;
	while (remaining >= 0) {
		if (poll(&pfd, 1, remaining > 0 ? remaining : 0) <= 0) {
			return -1;
		}
		ssize_t n = recv(fd, buf, max - 1, MSG_PEEK);
		if (n <= 0) return -1;
		char *nl = memchr(buf, '\n', n);
		if (nl != NULL) {
			size_t to_read = (size_t)(nl - buf + 1);
			ssize_t r = read(fd, buf, to_read);
			if (r <= 0) return -1;
			buf[r] = '\0';
			char *end = memchr(buf, '\n', r);
			if (end) *end = '\0';
			return (ssize_t)strlen(buf);
		}
		if ((size_t)n >= max - 1) {
			ssize_t discarded = read(fd, buf, max - 1);
			(void)discarded;
			return -1;
		}
		remaining -= 2;
	}
	return -1;
}

static void keyboard_handle_key(struct wl_listener *listener, void *data) {
	struct mint_keyboard *keyboard =
		wl_container_of(listener, keyboard, key);
	struct mint_server *server = keyboard->server;
	struct wlr_keyboard_key_event *event = data;
	struct wlr_seat *seat = server->seat;
	if (server->locked) {
		goto pass_key;
	}

	if (server->key_grabber != NULL) {
		uint32_t keycode = event->keycode + 8;
		const xkb_keysym_t *syms;
		int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
		uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
		uint32_t clean_mods = modifiers & (WLR_MODIFIER_SHIFT | WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT | WLR_MODIFIER_LOGO);
		xkb_keysym_t sym = (nsyms > 0) ? syms[0] : XKB_KEY_NoSymbol;

		const xkb_keysym_t *raw_syms;
		int nraw = xkb_keymap_key_get_syms_by_level(keyboard->wlr_keyboard->keymap, keycode, 0, 0, &raw_syms);
		xkb_keysym_t raw_sym = (nraw > 0) ? raw_syms[0] : sym;

		char msg[64];
		int len = snprintf(msg, sizeof(msg), "KEY %u %u %u %u\n", clean_mods, (uint32_t)sym, (uint32_t)raw_sym, event->state);

		int fd = server->key_grabber->fd;
		if (write(fd, msg, len) > 0) {
			char reply[256];
			if (read_reply_line(fd, reply, sizeof(reply), 10) > 0) {
				char *cmd = reply;
				while (*cmd == ' ' || *cmd == '\t') cmd++;

				if (strcmp(cmd, "PASS") == 0) {
					goto pass_key;
				} else if (strcmp(cmd, "SWALLOW") == 0 || strcmp(cmd, "NONE") == 0) {
					return;
				} else if (*cmd != '\0') {
					ipc_execute_command(server, server->key_grabber, cmd, NULL, 0);
					return;
				}
			} else {
				server->key_grabber = NULL;
			}
		} else {
			server->key_grabber = NULL;
		}
	}

pass_key:
	wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
	wlr_seat_keyboard_notify_key(seat, event->time_msec,
		event->keycode, event->state);
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
	struct mint_keyboard *keyboard =
		wl_container_of(listener, keyboard, destroy);
	wl_list_remove(&keyboard->modifiers.link);
	wl_list_remove(&keyboard->key.link);
	wl_list_remove(&keyboard->destroy.link);
	wl_list_remove(&keyboard->link);
	free(keyboard);
}

static void server_new_keyboard(struct mint_server *server,
		struct wlr_input_device *device) {
	struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

	struct mint_keyboard *keyboard = calloc(1, sizeof(*keyboard));
	if (!keyboard) {
		return;
	}
	keyboard->server = server;
	keyboard->wlr_keyboard = wlr_keyboard;

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL,
		XKB_KEYMAP_COMPILE_NO_FLAGS);

	wlr_keyboard_set_keymap(wlr_keyboard, keymap);
	xkb_keymap_unref(keymap);
	xkb_context_unref(context);
	wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

	keyboard->modifiers.notify = keyboard_handle_modifiers;
	wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);
	keyboard->key.notify = keyboard_handle_key;
	wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
	keyboard->destroy.notify = keyboard_handle_destroy;
	wl_signal_add(&device->events.destroy, &keyboard->destroy);

	wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);
	wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct mint_server *server,
		struct wlr_input_device *device) {
	wlr_cursor_attach_input_device(server->cursor, device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, new_input);
	struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		server_new_keyboard(server, device);
		break;
	case WLR_INPUT_DEVICE_POINTER:
		server_new_pointer(server, device);
		break;
	default:
		break;
	}

	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&server->keyboards)) {
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(
			listener, server, request_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	struct wlr_seat_client *focused_client =
		server->seat->pointer_state.focused_client;
	if (focused_client == event->seat_client) {
		wlr_cursor_set_surface(server->cursor, event->surface,
				event->hotspot_x, event->hotspot_y);
	}
}

static void seat_pointer_focus_change(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(
			listener, server, pointer_focus_change);
	struct wlr_seat_pointer_focus_change_event *event = data;
	if (event->new_surface == NULL) {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	}
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(
			listener, server, request_set_selection);
	struct wlr_seat_request_set_selection_event *event = data;
	wlr_seat_set_selection(server->seat, event->source, event->serial);
}

#if CONFIG_PRIMARY_SELECTION
static void seat_request_set_primary_selection(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(
			listener, server, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}
#endif

static struct mint_toplevel *desktop_toplevel_at(
		struct mint_server *server, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	struct wlr_scene_node *node = wlr_scene_node_at(
		&server->scene->tree.node, lx, ly, sx, sy);
	if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
		return NULL;
	}
	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
	struct wlr_scene_surface *scene_surface =
		wlr_scene_surface_try_from_buffer(scene_buffer);
	if (!scene_surface) {
		return NULL;
	}

	*surface = scene_surface->surface;
	struct wlr_scene_tree *tree = node->parent;
	while (tree != NULL && tree->node.data == NULL) {
		tree = tree->node.parent;
	}
	return tree ? tree->node.data : NULL;
}

static void process_cursor_motion(struct mint_server *server, uint32_t time) {
	double sx, sy;
	struct wlr_seat *seat = server->seat;
	struct wlr_surface *surface = NULL;
	struct mint_toplevel *toplevel = desktop_toplevel_at(server,
			server->cursor->x, server->cursor->y, &surface, &sx, &sy);
	if (!toplevel && !surface) {
		wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
	}
	if (surface) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, cursor_motion);
	struct wlr_pointer_motion_event *event = data;
	wlr_cursor_move(server->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, cursor_motion_absolute);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x,
		event->y);
	process_cursor_motion(server, event->time_msec);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, cursor_button);
	struct wlr_pointer_button_event *event = data;

	wlr_seat_pointer_notify_button(server->seat,
			event->time_msec, event->button, event->state);

	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		double sx, sy;
		struct wlr_surface *surface = NULL;
		struct mint_toplevel *toplevel = desktop_toplevel_at(server,
				server->cursor->x, server->cursor->y, &surface, &sx, &sy);
		if (server->locked) {
			if (surface != NULL) {
				struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
				if (keyboard != NULL) {
					wlr_seat_keyboard_notify_enter(server->seat, surface,
						keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
				} else {
					wlr_seat_keyboard_notify_enter(server->seat, surface, NULL, 0, NULL);
				}
			}
		} else if (toplevel != NULL) {
#if CONFIG_XWAYLAND
			if (toplevel_is_unmanaged(toplevel)) {
				if (toplevel->xwayland_surface &&
				    wlr_xwayland_surface_override_redirect_wants_focus(toplevel->xwayland_surface) &&
				    wlr_xwayland_surface_icccm_input_model(toplevel->xwayland_surface) != WLR_ICCCM_INPUT_MODEL_NONE) {
					struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
					if (surface && keyboard) {
						wlr_seat_keyboard_notify_enter(server->seat, surface,
							keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
					}
				}
			} else
#endif
			{
				focus_toplevel(server, toplevel);
			}
		}
	}
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	wlr_seat_pointer_notify_axis(server->seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, cursor_frame);
	wlr_seat_pointer_notify_frame(server->seat);
}

/* Output management */
static void output_frame(struct wl_listener *listener, void *data) {
	struct mint_output *output = wl_container_of(listener, output, frame);
	struct wlr_scene *scene = output->server->scene;
	struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
		scene, output->wlr_output);

	wlr_scene_output_commit(scene_output, NULL);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
	struct mint_output *output = wl_container_of(listener, output, request_state);
	const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
	arrange_layers(output);
#if CONFIG_SESSION_LOCK
	if (output->server->locked) {
		update_lock_bg(output->server);
		struct mint_session_lock_surface *surface;
		wl_list_for_each(surface, &output->server->lock_surfaces, link) {
			if (surface->output == output) {
				struct wlr_box box;
				wlr_output_layout_get_box(output->server->output_layout, output->wlr_output, &box);
				wlr_scene_node_set_position(&surface->scene_tree->node, box.x, box.y);
				wlr_session_lock_surface_v1_configure(surface->lock_surface, box.width, box.height);
				break;
			}
		}
	}
#endif
}

static void output_destroy(struct wl_listener *listener, void *data) {
	struct mint_output *output = wl_container_of(listener, output, destroy);

	wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	if (output->server->locked) {
		update_lock_bg(output->server);
	}
	free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);

	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}

	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	struct mint_output *output = calloc(1, sizeof(*output));
	if (!output) {
		wlr_log(WLR_ERROR, "failed to allocate mint_output");
		return;
	}
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	wl_list_init(&output->layers);

	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

	wl_list_insert(&server->outputs, &output->link);

	struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(
		server->output_layout, wlr_output);
	struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
	wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

	arrange_layers(output);
	if (server->locked) {
		update_lock_bg(server);
	}
}

#if CONFIG_LAYER_SHELL
/* Layer shell management (for external bars, docks, wallpapers) */
static void layer_surface_handle_commit(struct wl_listener *listener, void *data) {
	struct mint_layer_surface *layer_surface = wl_container_of(listener, layer_surface, surface_commit);
	struct wlr_layer_surface_v1 *wlr_layer = layer_surface->layer_surface;

	if (wlr_layer->initial_commit) {
		arrange_layers(layer_surface->output);
		return;
	}

	if (wlr_layer->current.layer != wlr_layer->pending.layer) {
		struct wlr_scene_tree *new_tree = get_layer_tree(layer_surface->server, wlr_layer->current.layer);
		wlr_scene_node_reparent(&layer_surface->scene_layer_surface->tree->node, new_tree);
	}

	if (wlr_layer->surface->mapped && (wlr_layer->current.committed != 0)) {
		arrange_layers(layer_surface->output);
	}
}

static void layer_surface_handle_map(struct wl_listener *listener, void *data) {
	struct mint_layer_surface *layer_surface = wl_container_of(listener, layer_surface, map);
	arrange_layers(layer_surface->output);

	if (!layer_surface->server->locked &&
			layer_surface->layer_surface->current.keyboard_interactive != ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE) {
		struct wlr_seat *seat = layer_surface->server->seat;
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
		if (keyboard) {
			wlr_seat_keyboard_notify_enter(seat, layer_surface->layer_surface->surface,
				keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
		}
	}
}

static void layer_surface_handle_unmap(struct wl_listener *listener, void *data) {
	struct mint_layer_surface *layer_surface = wl_container_of(listener, layer_surface, unmap);
	arrange_layers(layer_surface->output);

	if (layer_surface->server->seat->keyboard_state.focused_surface == layer_surface->layer_surface->surface) {
		if (!layer_surface->server->locked) {
			focus_toplevel(layer_surface->server, layer_surface->server->focused_toplevel);
		}
	}
}

static void layer_surface_handle_destroy(struct wl_listener *listener, void *data) {
	struct mint_layer_surface *layer_surface = wl_container_of(listener, layer_surface, destroy);

	wl_list_remove(&layer_surface->link);
	wl_list_remove(&layer_surface->destroy.link);
	wl_list_remove(&layer_surface->map.link);
	wl_list_remove(&layer_surface->unmap.link);
	wl_list_remove(&layer_surface->surface_commit.link);

	struct mint_output *output = layer_surface->output;
	free(layer_surface);

	arrange_layers(output);
}

static void server_new_layer_shell_surface(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(listener, server, new_layer_shell_surface);
	struct wlr_layer_surface_v1 *wlr_layer = data;

	if (!wlr_layer->output) {
		struct mint_output *output = get_active_output(server);
		if (!output) {
			wlr_layer_surface_v1_destroy(wlr_layer);
			return;
		}
		wlr_layer->output = output->wlr_output;
	}

	struct mint_output *output = wlr_layer->output->data;
	if (!output) {
		wlr_layer_surface_v1_destroy(wlr_layer);
		return;
	}

	struct mint_layer_surface *layer_surface = calloc(1, sizeof(*layer_surface));
	if (!layer_surface) {
		wlr_layer_surface_v1_destroy(wlr_layer);
		return;
	}

	layer_surface->server = server;
	layer_surface->output = output;
	layer_surface->layer_surface = wlr_layer;

	struct wlr_scene_tree *parent_tree = get_layer_tree(server, wlr_layer->pending.layer);
	layer_surface->scene_layer_surface = wlr_scene_layer_surface_v1_create(parent_tree, wlr_layer);
	if (!layer_surface->scene_layer_surface) {
		free(layer_surface);
		wlr_layer_surface_v1_destroy(wlr_layer);
		return;
	}
	wlr_layer->data = layer_surface->scene_layer_surface->tree;

	layer_surface->map.notify = layer_surface_handle_map;
	wl_signal_add(&wlr_layer->surface->events.map, &layer_surface->map);

	layer_surface->unmap.notify = layer_surface_handle_unmap;
	wl_signal_add(&wlr_layer->surface->events.unmap, &layer_surface->unmap);

	layer_surface->surface_commit.notify = layer_surface_handle_commit;
	wl_signal_add(&wlr_layer->surface->events.commit, &layer_surface->surface_commit);

	layer_surface->destroy.notify = layer_surface_handle_destroy;
	wl_signal_add(&wlr_layer->events.destroy, &layer_surface->destroy);

	wl_list_insert(&output->layers, &layer_surface->link);
}

#endif

/* XDG Shell management */
static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, map);
	struct mint_server *server = toplevel->server;

	toplevel->mapped = true;

#if CONFIG_XDG_DECORATION
	if (toplevel->decoration) {
		wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
#endif

	wl_list_insert(&server->toplevels, &toplevel->link);

	if (toplevel->xdg_toplevel->requested.fullscreen) {
		toplevel_set_fullscreen(toplevel, true);
	} else {
		wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel,
			WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
	}

	bool visible = (toplevel->workspace == server->current_workspace);
	wlr_scene_node_set_enabled(&toplevel->scene_tree->node, visible);

	try_auto_swallow(server, toplevel);

	arrange_windows(server);

#if CONFIG_SWALLOWING
	if (visible && toplevel->swallowed_by == NULL)
#else
	if (visible)
#endif
	{
		focus_toplevel(server, toplevel);
	}
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
	struct mint_server *server = toplevel->server;

	toplevel->mapped = false;

	if (toplevel->is_fullscreen) {
		toplevel_set_fullscreen(toplevel, false);
	}

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == toplevel) {
			server->last_focused_per_workspace[i] = NULL;
		}
	}

	bool was_focused = (server->focused_toplevel == toplevel);

	struct mint_toplevel *restored_term = NULL;
#if CONFIG_SWALLOWING
	if (toplevel->swallowing) {
		restored_term = toplevel->swallowing;
		unswallow_toplevel(server, toplevel);
	}
	if (toplevel->swallowed_by) {
		toplevel->swallowed_by->swallowing = NULL;
		toplevel->swallowed_by = NULL;
	}
#endif

	if (!wl_list_empty(&toplevel->link)) {
		wl_list_remove(&toplevel->link);
		wl_list_init(&toplevel->link);
	}

	if (was_focused) {
		server->focused_toplevel = NULL;
		struct mint_toplevel *next_focus = restored_term;
		if (!next_focus || !toplevel_is_mapped(next_focus) || next_focus->workspace != server->current_workspace) {
			next_focus = NULL;
			struct mint_toplevel *tl;
			wl_list_for_each(tl, &server->toplevels, link) {
				if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
					next_focus = tl;
					break;
				}
			}
		}
		focus_toplevel(server, next_focus);
	}

	arrange_windows(server);
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, commit);

	if (toplevel->xdg_toplevel->base->initial_commit) {
#if CONFIG_XDG_DECORATION
		if (toplevel->decoration) {
			wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration,
				WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
		}
#endif
		struct mint_output *output = get_active_output(toplevel->server);
		struct wlr_box full_area = {0};
		struct wlr_box area = {0};
		if (output) {
			wlr_output_layout_get_box(toplevel->server->output_layout, output->wlr_output, &full_area);
			area = output->usable_area;
			if (area.width <= 0 || area.height <= 0) {
				area = full_area;
			}
		}

		if (toplevel->xdg_toplevel->requested.fullscreen) {
			wlr_xdg_toplevel_set_fullscreen(toplevel->xdg_toplevel, true);
			wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, full_area.width, full_area.height);
		} else {
			wlr_xdg_toplevel_set_tiled(toplevel->xdg_toplevel,
				WLR_EDGE_TOP | WLR_EDGE_BOTTOM | WLR_EDGE_LEFT | WLR_EDGE_RIGHT);
			wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, area.width, area.height);
		}
		return;
	}

	if (toplevel->scene_tree && toplevel->xdg_toplevel->base->surface->mapped) {
		wlr_scene_node_set_position(&toplevel->scene_tree->node,
			toplevel->pending_x - toplevel->xdg_toplevel->base->geometry.x,
			toplevel->pending_y - toplevel->xdg_toplevel->base->geometry.y);
	}
}

static void toplevel_handle_set_title(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, set_title);
	if (toplevel == toplevel->server->focused_toplevel) {
		ipc_broadcast_state(toplevel->server);
	}
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
	struct mint_server *server = toplevel->server;

#if CONFIG_SWALLOWING
	if (toplevel->swallowing) {
		unswallow_toplevel(server, toplevel);
	}
	if (toplevel->swallowed_by) {
		toplevel->swallowed_by->swallowing = NULL;
		toplevel->swallowed_by = NULL;
	}
	struct mint_toplevel *tl_it;
	wl_list_for_each(tl_it, &server->toplevels, link) {
		if (tl_it->last_swallowed == toplevel) {
			tl_it->last_swallowed = NULL;
		}
	}
#endif

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == toplevel) {
			server->last_focused_per_workspace[i] = NULL;
		}
	}
	if (server->focused_toplevel == toplevel) {
		server->focused_toplevel = NULL;
		ipc_broadcast_state(server);
	}

	if (!wl_list_empty(&toplevel->link)) {
		wl_list_remove(&toplevel->link);
	}
	wl_list_remove(&toplevel->map.link);
	wl_list_remove(&toplevel->unmap.link);
	wl_list_remove(&toplevel->commit.link);
	wl_list_remove(&toplevel->destroy.link);
	wl_list_remove(&toplevel->request_move.link);
	wl_list_remove(&toplevel->request_resize.link);
	wl_list_remove(&toplevel->request_maximize.link);
	wl_list_remove(&toplevel->request_fullscreen.link);
#if CONFIG_XDG_DECORATION
	if (!wl_list_empty(&toplevel->decoration_destroy.link)) {
		wl_list_remove(&toplevel->decoration_destroy.link);
	}
	if (!wl_list_empty(&toplevel->decoration_request_mode.link)) {
		wl_list_remove(&toplevel->decoration_request_mode.link);
	}
#endif
	if (!wl_list_empty(&toplevel->set_title.link)) {
		wl_list_remove(&toplevel->set_title.link);
	}

	free(toplevel);
}

static void xdg_toplevel_request_move(struct wl_listener *listener, void *data) {
	/* No-op: only dwm-style tiling, no floating */
}

static void xdg_toplevel_request_resize(struct wl_listener *listener, void *data) {
	/* No-op: only dwm-style tiling, no floating */
}

static void xdg_toplevel_request_maximize(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_maximize);
	if (toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
	}
}

static void xdg_toplevel_request_fullscreen(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel =
		wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xdg_toplevel->base->initialized) {
		if (toplevel->is_fullscreen != toplevel->xdg_toplevel->requested.fullscreen) {
			toplevel_set_fullscreen(toplevel, toplevel->xdg_toplevel->requested.fullscreen);
		} else {
			wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
		}
	}
}

static void server_new_xdg_toplevel(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(listener, server, new_xdg_toplevel);
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	struct mint_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	if (!toplevel) {
		return;
	}
	toplevel->server = server;
	toplevel->type = MINT_TOPLEVEL_XDG;
	toplevel->xdg_toplevel = xdg_toplevel;
	toplevel->workspace = server->current_workspace;
	wl_list_init(&toplevel->link);
#if CONFIG_XDG_DECORATION
	wl_list_init(&toplevel->decoration_request_mode.link);
	wl_list_init(&toplevel->decoration_destroy.link);
#endif
#if CONFIG_XWAYLAND
	wl_list_init(&toplevel->request_configure.link);
	wl_list_init(&toplevel->request_activate.link);
	wl_list_init(&toplevel->associate.link);
	wl_list_init(&toplevel->dissociate.link);
#endif
	wl_list_init(&toplevel->set_title.link);
#if CONFIG_XWAYLAND
	wl_list_init(&toplevel->set_override_redirect.link);
	wl_list_init(&toplevel->set_geometry.link);
#endif

	toplevel->scene_tree =
		wlr_scene_xdg_surface_create(server->scene_tree_windows, xdg_toplevel->base);
	toplevel->scene_tree->node.data = toplevel;
	xdg_toplevel->base->data = toplevel->scene_tree;

	toplevel->map.notify = xdg_toplevel_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);
	toplevel->unmap.notify = xdg_toplevel_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);
	toplevel->commit.notify = xdg_toplevel_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

	toplevel->destroy.notify = xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

	toplevel->set_title.notify = toplevel_handle_set_title;
	wl_signal_add(&xdg_toplevel->events.set_title, &toplevel->set_title);

	toplevel->request_move.notify = xdg_toplevel_request_move;
	wl_signal_add(&xdg_toplevel->events.request_move, &toplevel->request_move);
	toplevel->request_resize.notify = xdg_toplevel_request_resize;
	wl_signal_add(&xdg_toplevel->events.request_resize, &toplevel->request_resize);
	toplevel->request_maximize.notify = xdg_toplevel_request_maximize;
	wl_signal_add(&xdg_toplevel->events.request_maximize, &toplevel->request_maximize);
	toplevel->request_fullscreen.notify = xdg_toplevel_request_fullscreen;
	wl_signal_add(&xdg_toplevel->events.request_fullscreen, &toplevel->request_fullscreen);
}

#if CONFIG_XWAYLAND
static void xwayland_surface_map(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, map);
	struct mint_server *server = toplevel->server;
	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

	toplevel->mapped = true;

	if (xsurface && xsurface->override_redirect) {
		toplevel->type = MINT_TOPLEVEL_XWAYLAND_UNMANAGED;
	}

	if (toplevel->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		toplevel->scene_tree = wlr_scene_subsurface_tree_create(
			server->scene_tree_top, xsurface->surface);
		if (toplevel->scene_tree) {
			toplevel->scene_tree->node.data = toplevel;
			wlr_scene_node_set_position(&toplevel->scene_tree->node, xsurface->x, xsurface->y);
		}
		if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface) &&
		    wlr_xwayland_surface_icccm_input_model(xsurface) != WLR_ICCCM_INPUT_MODEL_NONE) {
			struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
			if (xsurface->surface && keyboard) {
				wlr_seat_keyboard_notify_enter(server->seat, xsurface->surface,
					keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
			}
		}
		return;
	}

	toplevel->scene_tree = wlr_scene_subsurface_tree_create(
		server->scene_tree_windows, xsurface->surface);
	if (!toplevel->scene_tree) {
		return;
	}
	toplevel->scene_tree->node.data = toplevel;

	wl_list_insert(&server->toplevels, &toplevel->link);

	bool visible = (toplevel->workspace == server->current_workspace);
	wlr_scene_node_set_enabled(&toplevel->scene_tree->node, visible);

	if (xsurface->fullscreen) {
		toplevel_set_fullscreen(toplevel, true);
	}

	try_auto_swallow(server, toplevel);

	arrange_windows(server);

#if CONFIG_SWALLOWING
	if (visible && toplevel->swallowed_by == NULL)
#else
	if (visible)
#endif
	{
		focus_toplevel(server, toplevel);
	}
}

static void xwayland_surface_unmap(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);
	struct mint_server *server = toplevel->server;
	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

	toplevel->mapped = false;

	if (toplevel->is_fullscreen) {
		toplevel_set_fullscreen(toplevel, false);
	}

	if (toplevel_is_unmanaged(toplevel)) {
		if (toplevel->scene_tree) {
			wlr_scene_node_destroy(&toplevel->scene_tree->node);
			toplevel->scene_tree = NULL;
		}
		if (!wl_list_empty(&toplevel->link)) {
			wl_list_remove(&toplevel->link);
			wl_list_init(&toplevel->link);
		}
		for (int i = 0; i < NUM_WORKSPACES; i++) {
			if (server->last_focused_per_workspace[i] == toplevel) {
				server->last_focused_per_workspace[i] = NULL;
			}
		}
		if (server->focused_toplevel == toplevel) {
			server->focused_toplevel = NULL;
		}
		if (xsurface && xsurface->surface &&
		    server->seat->keyboard_state.focused_surface == xsurface->surface) {
			if (server->focused_toplevel) {
				focus_toplevel(server, server->focused_toplevel);
			} else {
				wlr_seat_keyboard_notify_clear_focus(server->seat);
			}
		}
		return;
	}

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == toplevel) {
			server->last_focused_per_workspace[i] = NULL;
		}
	}

	bool was_focused = (server->focused_toplevel == toplevel);

	struct mint_toplevel *restored_term = NULL;
#if CONFIG_SWALLOWING
	if (toplevel->swallowing) {
		restored_term = toplevel->swallowing;
		unswallow_toplevel(server, toplevel);
	}
	if (toplevel->swallowed_by) {
		toplevel->swallowed_by->swallowing = NULL;
		toplevel->swallowed_by = NULL;
	}
#endif

	if (!wl_list_empty(&toplevel->link)) {
		wl_list_remove(&toplevel->link);
		wl_list_init(&toplevel->link);
	}

	if (toplevel->scene_tree) {
		wlr_scene_node_destroy(&toplevel->scene_tree->node);
		toplevel->scene_tree = NULL;
	}

	if (was_focused) {
		server->focused_toplevel = NULL;
		struct mint_toplevel *next_focus = restored_term;
		if (!next_focus || !toplevel_is_mapped(next_focus) || next_focus->workspace != server->current_workspace) {
			next_focus = NULL;
			struct mint_toplevel *tl;
			wl_list_for_each(tl, &server->toplevels, link) {
				if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
					next_focus = tl;
					break;
				}
			}
		}
		focus_toplevel(server, next_focus);
	}

	arrange_windows(server);
}

static void xwayland_surface_associate(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, associate);
	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;

	if (!xsurface || !xsurface->surface) {
		return;
	}

	toplevel->map.notify = xwayland_surface_map;
	wl_signal_add(&xsurface->surface->events.map, &toplevel->map);

	toplevel->unmap.notify = xwayland_surface_unmap;
	wl_signal_add(&xsurface->surface->events.unmap, &toplevel->unmap);
}

static void xwayland_surface_dissociate(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, dissociate);

	if (toplevel->mapped) {
		xwayland_surface_unmap(&toplevel->unmap, NULL);
	}

	if (!wl_list_empty(&toplevel->map.link)) {
		wl_list_remove(&toplevel->map.link);
		wl_list_init(&toplevel->map.link);
	}
	if (!wl_list_empty(&toplevel->unmap.link)) {
		wl_list_remove(&toplevel->unmap.link);
		wl_list_init(&toplevel->unmap.link);
	}
}

static void xwayland_surface_request_configure(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, request_configure);
	struct wlr_xwayland_surface_configure_event *event = data;

	if (toplevel->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED) {
		wlr_xwayland_surface_configure(toplevel->xwayland_surface,
			event->x, event->y, event->width, event->height);
		if (toplevel->scene_tree) {
			wlr_scene_node_set_position(&toplevel->scene_tree->node, event->x, event->y);
		}
	} else {
		if (toplevel_is_mapped(toplevel)) {
			wlr_xwayland_surface_configure(toplevel->xwayland_surface,
				toplevel->pending_x, toplevel->pending_y,
				toplevel->pending_width, toplevel->pending_height);
		} else {
			wlr_xwayland_surface_configure(toplevel->xwayland_surface,
				event->x, event->y, event->width, event->height);
		}
	}
}

static void xwayland_surface_request_activate(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, request_activate);
	if (toplevel->type == MINT_TOPLEVEL_XWAYLAND && !toplevel_is_unmanaged(toplevel)) {
		if (toplevel->workspace == toplevel->server->current_workspace && toplevel_is_mapped(toplevel)) {
			focus_toplevel(toplevel->server, toplevel);
		}
	}
}

static void xwayland_surface_request_maximize(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, request_maximize);
	wlr_xwayland_surface_set_maximized(toplevel->xwayland_surface, false, false);
}

static void xwayland_surface_request_fullscreen(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, request_fullscreen);
	if (toplevel->xwayland_surface) {
		toplevel_set_fullscreen(toplevel, toplevel->xwayland_surface->fullscreen);
	}
}

static void xwayland_surface_set_override_redirect(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, set_override_redirect);
	struct mint_server *server = toplevel->server;
	struct wlr_xwayland_surface *xsurface = toplevel->xwayland_surface;
	bool new_override_redirect = xsurface ? xsurface->override_redirect : false;
	enum mint_toplevel_type new_type = new_override_redirect ?
		MINT_TOPLEVEL_XWAYLAND_UNMANAGED : MINT_TOPLEVEL_XWAYLAND;

	if (toplevel->type == new_type) {
		return;
	}

	toplevel->type = new_type;

	if (!toplevel->mapped) {
		return;
	}

	if (new_override_redirect) {
		/* Transition from managed to unmanaged */
		if (!wl_list_empty(&toplevel->link)) {
			wl_list_remove(&toplevel->link);
			wl_list_init(&toplevel->link);
		}
		for (int i = 0; i < NUM_WORKSPACES; i++) {
			if (server->last_focused_per_workspace[i] == toplevel) {
				server->last_focused_per_workspace[i] = NULL;
			}
		}
		if (toplevel->scene_tree) {
			wlr_scene_node_reparent(&toplevel->scene_tree->node, server->scene_tree_top);
			if (xsurface) {
				wlr_scene_node_set_position(&toplevel->scene_tree->node, xsurface->x, xsurface->y);
			}
		}
		if (server->focused_toplevel == toplevel) {
			server->focused_toplevel = NULL;
			struct mint_toplevel *next_focus = NULL;
			struct mint_toplevel *tl;
			wl_list_for_each(tl, &server->toplevels, link) {
				if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
					next_focus = tl;
					break;
				}
			}
			focus_toplevel(server, next_focus);
		}
		arrange_windows(server);
	} else {
		/* Transition from unmanaged to managed */
		if (toplevel->scene_tree) {
			wlr_scene_node_reparent(&toplevel->scene_tree->node, server->scene_tree_windows);
		}
		if (wl_list_empty(&toplevel->link)) {
			wl_list_insert(&server->toplevels, &toplevel->link);
		}
		bool visible = (toplevel->workspace == server->current_workspace);
		if (toplevel->scene_tree) {
			wlr_scene_node_set_enabled(&toplevel->scene_tree->node, visible);
		}
		arrange_windows(server);
		if (visible) {
			focus_toplevel(server, toplevel);
		}
	}
}

static void xwayland_surface_set_geometry(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, set_geometry);
	if (toplevel->type == MINT_TOPLEVEL_XWAYLAND_UNMANAGED && toplevel->scene_tree) {
		wlr_scene_node_set_position(&toplevel->scene_tree->node,
			toplevel->xwayland_surface->x, toplevel->xwayland_surface->y);
	}
}

static void xwayland_surface_destroy(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);
	struct mint_server *server = toplevel->server;

	if (toplevel->mapped) {
		xwayland_surface_unmap(&toplevel->unmap, NULL);
	}

#if CONFIG_SWALLOWING
	if (toplevel->swallowing) {
		unswallow_toplevel(server, toplevel);
	}
	if (toplevel->swallowed_by) {
		toplevel->swallowed_by->swallowing = NULL;
		toplevel->swallowed_by = NULL;
	}
	struct mint_toplevel *tl_it;
	wl_list_for_each(tl_it, &server->toplevels, link) {
		if (tl_it->last_swallowed == toplevel) {
			tl_it->last_swallowed = NULL;
		}
	}
#endif

	for (int i = 0; i < NUM_WORKSPACES; i++) {
		if (server->last_focused_per_workspace[i] == toplevel) {
			server->last_focused_per_workspace[i] = NULL;
		}
	}
	if (server->focused_toplevel == toplevel) {
		server->focused_toplevel = NULL;
		ipc_broadcast_state(server);
	}

	if (!wl_list_empty(&toplevel->link)) {
		wl_list_remove(&toplevel->link);
	}
	if (!wl_list_empty(&toplevel->map.link)) {
		wl_list_remove(&toplevel->map.link);
	}
	if (!wl_list_empty(&toplevel->unmap.link)) {
		wl_list_remove(&toplevel->unmap.link);
	}
	if (!wl_list_empty(&toplevel->destroy.link)) {
		wl_list_remove(&toplevel->destroy.link);
	}
	if (!wl_list_empty(&toplevel->request_configure.link)) {
		wl_list_remove(&toplevel->request_configure.link);
	}
	if (!wl_list_empty(&toplevel->request_activate.link)) {
		wl_list_remove(&toplevel->request_activate.link);
	}
	if (!wl_list_empty(&toplevel->request_maximize.link)) {
		wl_list_remove(&toplevel->request_maximize.link);
	}
	if (!wl_list_empty(&toplevel->request_fullscreen.link)) {
		wl_list_remove(&toplevel->request_fullscreen.link);
	}
	if (!wl_list_empty(&toplevel->associate.link)) {
		wl_list_remove(&toplevel->associate.link);
	}
	if (!wl_list_empty(&toplevel->dissociate.link)) {
		wl_list_remove(&toplevel->dissociate.link);
	}
	if (!wl_list_empty(&toplevel->set_title.link)) {
		wl_list_remove(&toplevel->set_title.link);
	}
	if (!wl_list_empty(&toplevel->set_override_redirect.link)) {
		wl_list_remove(&toplevel->set_override_redirect.link);
	}
	if (!wl_list_empty(&toplevel->set_geometry.link)) {
		wl_list_remove(&toplevel->set_geometry.link);
	}

	if (toplevel->scene_tree) {
		wlr_scene_node_destroy(&toplevel->scene_tree->node);
		toplevel->scene_tree = NULL;
	}

	if (toplevel->xwayland_surface) {
		toplevel->xwayland_surface->data = NULL;
	}

	free(toplevel);
}

static void handle_new_xwayland_surface(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(listener, server, new_xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;

	struct mint_toplevel *toplevel = calloc(1, sizeof(*toplevel));
	if (!toplevel) {
		return;
	}
	toplevel->server = server;
	toplevel->xwayland_surface = xsurface;
	toplevel->type = xsurface->override_redirect ?
		MINT_TOPLEVEL_XWAYLAND_UNMANAGED : MINT_TOPLEVEL_XWAYLAND;
	toplevel->workspace = server->current_workspace;

	wl_list_init(&toplevel->link);
#if CONFIG_XDG_DECORATION
	wl_list_init(&toplevel->decoration_request_mode.link);
	wl_list_init(&toplevel->decoration_destroy.link);
#endif
	wl_list_init(&toplevel->map.link);
	wl_list_init(&toplevel->unmap.link);
	wl_list_init(&toplevel->commit.link);
	wl_list_init(&toplevel->destroy.link);
	wl_list_init(&toplevel->request_move.link);
	wl_list_init(&toplevel->request_resize.link);
	wl_list_init(&toplevel->request_maximize.link);
	wl_list_init(&toplevel->request_fullscreen.link);
	wl_list_init(&toplevel->request_configure.link);
	wl_list_init(&toplevel->request_activate.link);
	wl_list_init(&toplevel->associate.link);
	wl_list_init(&toplevel->dissociate.link);
	wl_list_init(&toplevel->set_title.link);
	wl_list_init(&toplevel->set_override_redirect.link);
	wl_list_init(&toplevel->set_geometry.link);

	xsurface->data = toplevel;

	toplevel->destroy.notify = xwayland_surface_destroy;
	wl_signal_add(&xsurface->events.destroy, &toplevel->destroy);

	toplevel->set_title.notify = toplevel_handle_set_title;
	wl_signal_add(&xsurface->events.set_title, &toplevel->set_title);

	toplevel->request_configure.notify = xwayland_surface_request_configure;
	wl_signal_add(&xsurface->events.request_configure, &toplevel->request_configure);

	toplevel->request_activate.notify = xwayland_surface_request_activate;
	wl_signal_add(&xsurface->events.request_activate, &toplevel->request_activate);

	toplevel->request_maximize.notify = xwayland_surface_request_maximize;
	wl_signal_add(&xsurface->events.request_maximize, &toplevel->request_maximize);

	toplevel->request_fullscreen.notify = xwayland_surface_request_fullscreen;
	wl_signal_add(&xsurface->events.request_fullscreen, &toplevel->request_fullscreen);

	toplevel->associate.notify = xwayland_surface_associate;
	wl_signal_add(&xsurface->events.associate, &toplevel->associate);

	toplevel->dissociate.notify = xwayland_surface_dissociate;
	wl_signal_add(&xsurface->events.dissociate, &toplevel->dissociate);

	toplevel->set_override_redirect.notify = xwayland_surface_set_override_redirect;
	wl_signal_add(&xsurface->events.set_override_redirect, &toplevel->set_override_redirect);

	toplevel->set_geometry.notify = xwayland_surface_set_geometry;
	wl_signal_add(&xsurface->events.set_geometry, &toplevel->set_geometry);

	if (xsurface->surface != NULL) {
		xwayland_surface_associate(&toplevel->associate, NULL);
	}
}

static void handle_xwayland_ready(struct wl_listener *listener, void *data) {
	struct mint_server *server = wl_container_of(listener, server, xwayland_ready);
	if (server->seat) {
		wlr_xwayland_set_seat(server->xwayland, server->seat);
	}
	if (server->xwayland->display_name) {
		setenv("DISPLAY", server->xwayland->display_name, true);
	}
}

#endif

static void xdg_popup_commit(struct wl_listener *listener, void *data) {
	struct mint_popup *popup = wl_container_of(listener, popup, commit);
	if (popup->xdg_popup->base->initial_commit) {
		wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
	}
}

static void xdg_popup_destroy(struct wl_listener *listener, void *data) {
	struct mint_popup *popup = wl_container_of(listener, popup, destroy);
	wl_list_remove(&popup->commit.link);
	wl_list_remove(&popup->destroy.link);
	free(popup);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup *xdg_popup = data;
	struct mint_popup *popup = calloc(1, sizeof(*popup));
	if (!popup) {
		return;
	}
	popup->xdg_popup = xdg_popup;

	struct wlr_scene_tree *parent_tree = NULL;
	struct wlr_xdg_surface *parent_xdg = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
	if (parent_xdg != NULL) {
		parent_tree = parent_xdg->data;
	} else {
#if CONFIG_LAYER_SHELL
		struct wlr_layer_surface_v1 *parent_layer = wlr_layer_surface_v1_try_from_wlr_surface(xdg_popup->parent);
		if (parent_layer != NULL) {
			parent_tree = parent_layer->data;
		}
#endif
	}
	if (parent_tree == NULL) {
		free(popup);
		return;
	}
	xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

	popup->commit.notify = xdg_popup_commit;
	wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

	popup->destroy.notify = xdg_popup_destroy;
	wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}

#if CONFIG_XDG_DECORATION
/* Server-side decoration negotiation (request clients not to draw titlebars) */
static void xdg_decoration_handle_request_mode(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, decoration_request_mode);
	if (toplevel->xdg_toplevel && toplevel->xdg_toplevel->base->initialized) {
		wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
}

static void xdg_decoration_handle_destroy(struct wl_listener *listener, void *data) {
	struct mint_toplevel *toplevel = wl_container_of(listener, toplevel, decoration_destroy);
	wl_list_remove(&toplevel->decoration_destroy.link);
	wl_list_init(&toplevel->decoration_destroy.link);
	wl_list_remove(&toplevel->decoration_request_mode.link);
	wl_list_init(&toplevel->decoration_request_mode.link);
	toplevel->decoration = NULL;
}

static void server_new_xdg_decoration(struct wl_listener *listener, void *data) {
	(void)listener;
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;
	if (!wlr_deco->toplevel || !wlr_deco->toplevel->base || !wlr_deco->toplevel->base->data) {
		return;
	}
	struct wlr_scene_tree *tree = wlr_deco->toplevel->base->data;
	struct mint_toplevel *toplevel = tree->node.data;
	if (!toplevel) {
		return;
	}

	toplevel->decoration = wlr_deco;

	toplevel->decoration_request_mode.notify = xdg_decoration_handle_request_mode;
	wl_signal_add(&wlr_deco->events.request_mode, &toplevel->decoration_request_mode);
	toplevel->decoration_destroy.notify = xdg_decoration_handle_destroy;
	wl_signal_add(&wlr_deco->events.destroy, &toplevel->decoration_destroy);

	if (wlr_deco->toplevel->base->initialized) {
		wlr_xdg_toplevel_decoration_v1_set_mode(wlr_deco,
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}
}

#endif

/* IPC implementation */
static void ipc_client_destroy(struct mint_ipc_client *client) {
	if (client->server->key_grabber == client) {
		client->server->key_grabber = NULL;
	}
	wl_list_remove(&client->link);
	if (client->event_source) {
		wl_event_source_remove(client->event_source);
	}
	close(client->fd);
	free(client);
}

static void ipc_reply(char *resp, size_t resp_size, const char *fmt, ...) {
	if (!resp || resp_size == 0) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	vsnprintf(resp, resp_size, fmt, args);
	va_end(args);
}

static void sanitize_title_for_state(const char *src, char *dst, size_t dst_size) {
	if (!dst || dst_size == 0) return;
	if (!src) {
		dst[0] = '\0';
		return;
	}
	size_t o = 0;
	for (size_t i = 0; src[i] != '\0' && o + 1 < dst_size; i++) {
		char c = src[i];
		if (c == '\n' || c == '\r') {
			dst[o++] = ' ';
		} else {
			dst[o++] = c;
		}
	}
	dst[o] = '\0';
}

static void json_escape_string(const char *src, char *dst, size_t dst_size) {
	if (!dst || dst_size == 0) return;
	if (!src) {
		dst[0] = '\0';
		return;
	}
	size_t o = 0;
	for (size_t i = 0; src[i] != '\0' && o + 1 < dst_size; i++) {
		unsigned char c = (unsigned char)src[i];
		if (c == '"' || c == '\\') {
			if (o + 2 >= dst_size) break;
			dst[o++] = '\\';
			dst[o++] = (char)c;
		} else if (c == '\n') {
			if (o + 2 >= dst_size) break;
			dst[o++] = '\\';
			dst[o++] = 'n';
		} else if (c == '\r') {
			if (o + 2 >= dst_size) break;
			dst[o++] = '\\';
			dst[o++] = 'r';
		} else if (c == '\t') {
			if (o + 2 >= dst_size) break;
			dst[o++] = '\\';
			dst[o++] = 't';
		} else if (c < 32) {
			continue;
		} else {
			dst[o++] = (char)c;
		}
	}
	dst[o] = '\0';
}

static void ipc_push_state(struct mint_server *server, struct mint_ipc_client *client) {
	const char *raw_title = server->focused_toplevel ?
		toplevel_get_title(server->focused_toplevel) : "";
	char title[256];
	sanitize_title_for_state(raw_title, title, sizeof(title));
	char msg[512];
	int len = snprintf(msg, sizeof(msg), "STATE %u %s\n",
		server->current_workspace, title);
	ssize_t w = write(client->fd, msg, len);
	if (w <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		ipc_client_destroy(client);
	}
}

static void ipc_broadcast_state(struct mint_server *server) {
	const char *raw_title = server->focused_toplevel ?
		toplevel_get_title(server->focused_toplevel) : "";
	char title[256];
	sanitize_title_for_state(raw_title, title, sizeof(title));
	char msg[512];
	int len = snprintf(msg, sizeof(msg), "STATE %u %s\n",
		server->current_workspace, title);

	struct mint_ipc_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &server->ipc_clients, link) {
		if (client->subscribed) {
			ssize_t w = write(client->fd, msg, len);
			if (w <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
				ipc_client_destroy(client);
			}
		}
	}
}

static void ipc_execute_command(struct mint_server *server,
		struct mint_ipc_client *client,
		const char *cmd, char *resp, size_t resp_size) {
	while (*cmd == ' ' || *cmd == '\t') cmd++;

	if (*cmd == '\0') {
		ipc_reply(resp, resp_size, "ERROR empty command\n");
		return;
	}

	if (server->locked) {
		if (strcmp(cmd, "status") != 0 && strcmp(cmd, "get_workspace") != 0 &&
				strcmp(cmd, "get_workspaces") != 0 && strcmp(cmd, "subscribe") != 0) {
			ipc_reply(resp, resp_size, "ERROR compositor is locked\n");
			return;
		}
	}

	if (strcmp(cmd, "grab_keys") == 0) {
		server->key_grabber = client;
		if (client && client->event_source) {
			wl_event_source_remove(client->event_source);
			client->event_source = NULL;
		}
		ipc_reply(resp, resp_size, "OK key grab active\n");
		return;
	}

	if (strcmp(cmd, "ungrab_keys") == 0) {
		if (server->key_grabber == client) {
			server->key_grabber = NULL;
		}
		ipc_reply(resp, resp_size, "OK key grab released\n");
		return;
	}

	if (strcmp(cmd, "subscribe") == 0) {
		if (client) {
			client->subscribed = true;
			ipc_push_state(server, client);
		}
		return;
	}

	if (strncmp(cmd, "sh ", 3) == 0 || strncmp(cmd, "run ", 4) == 0 || strncmp(cmd, "exec ", 5) == 0) {
		const char *sh_cmd = cmd;
		while (*sh_cmd != ' ' && *sh_cmd != '\0') sh_cmd++;
		while (*sh_cmd == ' ') sh_cmd++;
		if (*sh_cmd != '\0') {
			pid_t pid = fork();
			if (pid < 0) {
				ipc_reply(resp, resp_size, "ERROR fork failed\n");
			} else if (pid == 0) {
				setsid();
				execl("/bin/sh", "/bin/sh", "-c", sh_cmd, (char *)NULL);
				_exit(1);
			} else {
				ipc_reply(resp, resp_size, "OK launched '%s'\n", sh_cmd);
			}
		} else {
			ipc_reply(resp, resp_size, "ERROR empty command\n");
		}
		return;
	}

	if (strncmp(cmd, "workspace", 9) == 0 || strncmp(cmd, "ws", 2) == 0) {
		const char *arg = cmd;
		while (*arg != ' ' && *arg != '\0') arg++;
		while (*arg == ' ') arg++;

		if (strcmp(arg, "next") == 0) {
			unsigned int next_ws = (server->current_workspace % NUM_WORKSPACES) + 1;
			change_workspace(server, next_ws);
			ipc_reply(resp, resp_size, "OK workspace %u\n", next_ws);
		} else if (strcmp(arg, "prev") == 0) {
			unsigned int prev_ws = (server->current_workspace == 1) ? NUM_WORKSPACES : server->current_workspace - 1;
			change_workspace(server, prev_ws);
			ipc_reply(resp, resp_size, "OK workspace %u\n", prev_ws);
		} else if (strcmp(arg, "back") == 0 || strcmp(arg, "previous") == 0 ||
		           strcmp(arg, "toggle") == 0 || strcmp(arg, "last") == 0 ||
		           strcmp(arg, "back_and_forth") == 0) {
			unsigned int target_ws = server->prev_workspace;
			change_workspace(server, target_ws);
			ipc_reply(resp, resp_size, "OK workspace %u\n", server->current_workspace);
		} else {
			int ws = atoi(arg);
			if (ws >= 1 && ws <= NUM_WORKSPACES) {
				change_workspace(server, (unsigned int)ws);
				ipc_reply(resp, resp_size, "OK workspace %u\n", (unsigned int)ws);
			} else {
				ipc_reply(resp, resp_size, "ERROR invalid workspace (1-%d, next, prev, back)\n", NUM_WORKSPACES);
			}
		}
		return;
	}

	if (strncmp(cmd, "moveto", 6) == 0 || strncmp(cmd, "move-to-workspace", 17) == 0 || strncmp(cmd, "sendto", 6) == 0) {
		const char *arg = cmd;
		while (*arg != ' ' && *arg != '\0') arg++;
		while (*arg == ' ') arg++;

		int ws;
		if (strcmp(arg, "back") == 0 || strcmp(arg, "previous") == 0 ||
		    strcmp(arg, "toggle") == 0 || strcmp(arg, "last") == 0 ||
		    strcmp(arg, "back_and_forth") == 0) {
			ws = (int)server->prev_workspace;
		} else {
			ws = atoi(arg);
		}

		if (ws >= 1 && ws <= NUM_WORKSPACES) {
			if (server->focused_toplevel != NULL) {
				move_to_workspace(server, (unsigned int)ws);
				ipc_reply(resp, resp_size, "OK moved to workspace %u\n", (unsigned int)ws);
			} else {
				ipc_reply(resp, resp_size, "ERROR no focused window\n");
			}
		} else {
			ipc_reply(resp, resp_size, "ERROR invalid workspace (1-%d, back)\n", NUM_WORKSPACES);
		}
		return;
	}

	if (strcmp(cmd, "close") == 0 || strcmp(cmd, "kill") == 0) {
		if (server->focused_toplevel != NULL) {
			toplevel_close(server->focused_toplevel);
			ipc_reply(resp, resp_size, "OK window closed\n");
		} else {
			ipc_reply(resp, resp_size, "ERROR no focused window\n");
		}
		return;
	}

	if (strncmp(cmd, "focus", 5) == 0) {
		const char *arg = cmd + 5;
		while (*arg == ' ') arg++;
		if (*arg == '\0' || strcmp(arg, "next") == 0) {
			focus_cycle(server, false);
			ipc_reply(resp, resp_size, "OK focus next\n");
		} else if (strcmp(arg, "prev") == 0) {
			focus_cycle(server, true);
			ipc_reply(resp, resp_size, "OK focus prev\n");
		} else if (strcmp(arg, "master") == 0) {
			focus_master(server);
			ipc_reply(resp, resp_size, "OK focus master\n");
		} else {
			ipc_reply(resp, resp_size, "ERROR unknown focus target (next/prev/master)\n");
		}
		return;
	}

	if (strcmp(cmd, "swap") == 0 || strcmp(cmd, "zoom") == 0 || strcmp(cmd, "swap master") == 0) {
		swap_master(server);
		ipc_reply(resp, resp_size, "OK swapped master\n");
		return;
	}

	if (strcmp(cmd, "fullscreen") == 0 || strcmp(cmd, "toggle_fullscreen") == 0) {
		if (server->focused_toplevel != NULL) {
			toplevel_set_fullscreen(server->focused_toplevel, !server->focused_toplevel->is_fullscreen);
			ipc_reply(resp, resp_size, "OK fullscreen %s\n",
				server->focused_toplevel->is_fullscreen ? "on" : "off");
		} else {
			ipc_reply(resp, resp_size, "ERROR no focused window\n");
		}
		return;
	}

#if CONFIG_SWALLOWING
	if (strcmp(cmd, "toggle_swallow") == 0 || strcmp(cmd, "swallow") == 0) {
		if (server->focused_toplevel != NULL) {
			bool swallowed = toplevel_toggle_swallow(server, server->focused_toplevel);
			ipc_reply(resp, resp_size, "OK swallow %s\n", swallowed ? "on" : "off");
		} else {
			ipc_reply(resp, resp_size, "ERROR no focused window\n");
		}
		return;
	}

	if (strcmp(cmd, "get_swallow") == 0) {
		bool is_swallowing = (server->focused_toplevel != NULL && server->focused_toplevel->swallowing != NULL);
		ipc_reply(resp, resp_size, "%d\n", is_swallowing ? 1 : 0);
		return;
	}

	if (strcmp(cmd, "toggle_auto_swallow") == 0) {
		server->auto_swallow = !server->auto_swallow;
		ipc_reply(resp, resp_size, "OK auto_swallow %s\n", server->auto_swallow ? "on" : "off");
		return;
	}

	if (strncmp(cmd, "auto_swallow", 12) == 0) {
		const char *arg = cmd + 12;
		while (*arg == ' ') arg++;
		if (*arg == '\0') {
			ipc_reply(resp, resp_size, "%d\n", server->auto_swallow ? 1 : 0);
		} else if (strcmp(arg, "toggle") == 0) {
			server->auto_swallow = !server->auto_swallow;
			ipc_reply(resp, resp_size, "OK auto_swallow %s\n", server->auto_swallow ? "on" : "off");
		} else if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0) {
			server->auto_swallow = true;
			ipc_reply(resp, resp_size, "OK auto_swallow on\n");
		} else if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "false") == 0) {
			server->auto_swallow = false;
			ipc_reply(resp, resp_size, "OK auto_swallow off\n");
		} else {
			ipc_reply(resp, resp_size, "ERROR expected on/off/toggle\n");
		}
		return;
	}
#endif

	if (strncmp(cmd, "mfact", 5) == 0) {
		const char *arg = cmd + 5;
		while (*arg == ' ') arg++;
		if (*arg == '+' || *arg == '-') {
			double delta = atof(arg);
			server->mfact += delta;
		} else if (*arg != '\0') {
			server->mfact = atof(arg);
		}
		if (server->mfact < 0.1) server->mfact = 0.1;
		if (server->mfact > 0.9) server->mfact = 0.9;
		arrange_windows(server);
		ipc_reply(resp, resp_size, "OK mfact %.2f\n", server->mfact);
		return;
	}

#if CONFIG_GAPS
	if (strcmp(cmd, "toggle_gaps") == 0) {
		server->gaps_enabled = !server->gaps_enabled;
		arrange_windows(server);
		ipc_reply(resp, resp_size, "OK gaps %s\n", server->gaps_enabled ? "on" : "off");
		return;
	}

	if (strncmp(cmd, "gaps", 4) == 0) {
		const char *arg = cmd + 4;
		while (*arg == ' ') arg++;
		if (*arg == '\0') {
			ipc_reply(resp, resp_size, "%d\n", server->gaps_enabled ? server->gaps : 0);
		} else if (strcmp(arg, "toggle") == 0) {
			server->gaps_enabled = !server->gaps_enabled;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK gaps %s\n", server->gaps_enabled ? "on" : "off");
		} else if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0) {
			server->gaps_enabled = true;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK gaps on\n");
		} else if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "false") == 0) {
			server->gaps_enabled = false;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK gaps off\n");
		} else if (*arg == '+' || *arg == '-') {
			int delta = atoi(arg);
			server->gaps += delta;
			if (server->gaps < 0) server->gaps = 0;
			if (server->gaps > 100) server->gaps = 100;
			server->gaps_enabled = (server->gaps > 0);
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK gaps %d\n", server->gaps);
		} else {
			int val = atoi(arg);
			if (val >= 0 && val <= 100) {
				server->gaps = val;
				server->gaps_enabled = (val > 0);
				arrange_windows(server);
				ipc_reply(resp, resp_size, "OK gaps %d\n", server->gaps);
			} else {
				ipc_reply(resp, resp_size, "ERROR invalid gaps value (0-100, +/-N, on/off/toggle)\n");
			}
		}
		return;
	}

	if (strcmp(cmd, "get_gaps") == 0) {
		ipc_reply(resp, resp_size, "%d\n", server->gaps_enabled ? server->gaps : 0);
		return;
	}

	if (strcmp(cmd, "toggle_smart_gaps") == 0) {
		server->smart_gaps = !server->smart_gaps;
		arrange_windows(server);
		ipc_reply(resp, resp_size, "OK smart_gaps %s\n", server->smart_gaps ? "on" : "off");
		return;
	}

	if (strncmp(cmd, "smart_gaps", 10) == 0) {
		const char *arg = cmd + 10;
		while (*arg == ' ') arg++;
		if (*arg == '\0') {
			ipc_reply(resp, resp_size, "%d\n", server->smart_gaps ? 1 : 0);
		} else if (strcmp(arg, "toggle") == 0) {
			server->smart_gaps = !server->smart_gaps;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK smart_gaps %s\n", server->smart_gaps ? "on" : "off");
		} else if (strcmp(arg, "on") == 0 || strcmp(arg, "1") == 0 || strcmp(arg, "true") == 0) {
			server->smart_gaps = true;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK smart_gaps on\n");
		} else if (strcmp(arg, "off") == 0 || strcmp(arg, "0") == 0 || strcmp(arg, "false") == 0) {
			server->smart_gaps = false;
			arrange_windows(server);
			ipc_reply(resp, resp_size, "OK smart_gaps off\n");
		} else {
			ipc_reply(resp, resp_size, "ERROR expected on/off/toggle\n");
		}
		return;
	}

	if (strcmp(cmd, "get_smart_gaps") == 0) {
		ipc_reply(resp, resp_size, "%d\n", server->smart_gaps ? 1 : 0);
		return;
	}
#endif

	if (strcmp(cmd, "get_workspace") == 0) {
		ipc_reply(resp, resp_size, "%u\n", server->current_workspace);
		return;
	}

	if (strcmp(cmd, "get_workspaces") == 0) {
		char buf[128] = "";
		char *p = buf;
		for (unsigned int i = 1; i <= NUM_WORKSPACES; i++) {
			if (i == server->current_workspace) {
				p += snprintf(p, sizeof(buf) - (p - buf), "[%u] ", i);
			} else {
				p += snprintf(p, sizeof(buf) - (p - buf), "%u ", i);
			}
		}
		if (p > buf && *(p - 1) == ' ') *(p - 1) = '\0';
		ipc_reply(resp, resp_size, "%s\n", buf);
		return;
	}

	if (strcmp(cmd, "get_title") == 0) {
		const char *title = "";
		if (server->focused_toplevel) {
			title = toplevel_get_title(server->focused_toplevel);
		}
		ipc_reply(resp, resp_size, "%s\n", title ? title : "");
		return;
	}

	if (strcmp(cmd, "status") == 0) {
		int count = 0;
		struct mint_toplevel *tl;
		wl_list_for_each(tl, &server->toplevels, link) {
			if (toplevel_is_mapped(tl) && tl->workspace == server->current_workspace) {
				count++;
			}
		}
		const char *title = server->focused_toplevel ?
			toplevel_get_title(server->focused_toplevel) : "";
		char escaped_title[512];
		json_escape_string(title ? title : "", escaped_title, sizeof(escaped_title));
#if CONFIG_SWALLOWING
		bool is_swallowing = (server->focused_toplevel != NULL && server->focused_toplevel->swallowing != NULL);
		bool auto_swallow_val = server->auto_swallow;
#else
		bool is_swallowing = false;
		bool auto_swallow_val = false;
#endif
#if CONFIG_GAPS
		int gaps_val = server->gaps;
		bool gaps_enabled_val = server->gaps_enabled;
		bool smart_gaps_val = server->smart_gaps;
#else
		int gaps_val = 0;
		bool gaps_enabled_val = false;
		bool smart_gaps_val = false;
#endif
		ipc_reply(resp, resp_size, "{\"workspace\":%u,\"windows\":%d,\"title\":\"%s\",\"locked\":%s,\"swallowing\":%s,\"auto_swallow\":%s,\"gaps\":%d,\"gaps_enabled\":%s,\"smart_gaps\":%s}\n",
			server->current_workspace, count, escaped_title, server->locked ? "true" : "false",
			is_swallowing ? "true" : "false", auto_swallow_val ? "true" : "false",
			gaps_val, gaps_enabled_val ? "true" : "false", smart_gaps_val ? "true" : "false");
		return;
	}

	if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
		ipc_reply(resp, resp_size, "OK exiting\n");
		wl_display_terminate(server->wl_display);
		return;
	}

	if (strcmp(cmd, "help") == 0) {
		ipc_reply(resp, resp_size,
			"Commands:\n"
			"  sh <cmd>                  Run shell command\n"
			"  run <cmd>                 Alias for sh\n"
			"  workspace <1-9|next|prev|back> Change workspace\n"
			"  moveto <1-9|back>         Move focused window to workspace\n"
			"  close                     Close focused window\n"
			"  focus <next|prev|master>  Change window focus\n"
			"  swap                      Swap focused window with master\n"
			"  fullscreen                Toggle fullscreen for focused window\n"
			"  toggle_swallow            Toggle window swallow\n"
			"  get_swallow               Get swallowing state (0 or 1)\n"
			"  toggle_auto_swallow       Toggle auto-swallow on/off\n"
			"  auto_swallow <on|off|toggle> Control auto-swallow\n"
#if CONFIG_GAPS
			"  toggle_gaps               Toggle gaps on/off\n"
			"  gaps <0-100|+N|-N|on|off> Control gap size or state\n"
			"  get_gaps                  Get current gap size\n"
			"  toggle_smart_gaps         Toggle smart gaps on/off\n"
			"  smart_gaps <on|off|toggle> Control smart gaps\n"
			"  get_smart_gaps            Get smart gaps state (0 or 1)\n"
#endif
			"  mfact <factor>            Change master factor (0.1 - 0.9)\n"
			"  get_workspace             Get current workspace number\n"
			"  get_workspaces            Get workspace list with [active]\n"
			"  get_title                 Get active window title\n"
			"  subscribe                 Subscribe to state change events\n"
			"  status                    Get status JSON\n"
			"  exit                      Exit compositor\n");
		return;
	}

	ipc_reply(resp, resp_size, "ERROR unknown command '%s', try 'help'\n", cmd);
}

static int ipc_handle_client_data(int fd, uint32_t mask, void *data) {
	struct mint_ipc_client *client = data;

	if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
		ipc_client_destroy(client);
		return 0;
	}

	char buf[512];
	ssize_t n = read(fd, buf, sizeof(buf) - 1);
	if (n <= 0) {
		ipc_client_destroy(client);
		return 0;
	}
	buf[n] = '\0';

	size_t available = sizeof(client->buffer) - client->len - 1;
	if ((size_t)n > available) {
		n = (ssize_t)available;
	}
	memcpy(client->buffer + client->len, buf, n);
	client->len += n;
	client->buffer[client->len] = '\0';

	char *newline;
	while ((newline = strchr(client->buffer, '\n')) != NULL) {
		*newline = '\0';
		char *cmd = client->buffer;
		while (*cmd == ' ' || *cmd == '\t') cmd++;
		char *end = cmd + strlen(cmd) - 1;
		while (end >= cmd && (*end == ' ' || *end == '\t' || *end == '\r')) {
			*end = '\0';
			end--;
		}

		if (*cmd != '\0') {
			char response[1024] = {0};
			ipc_execute_command(client->server, client, cmd, response, sizeof(response));
			if (response[0] != '\0') {
				size_t rlen = strlen(response);
				ssize_t w = write(client->fd, response, rlen);
				(void)w;
			}
		}

		size_t processed = (newline - client->buffer) + 1;
		memmove(client->buffer, client->buffer + processed, client->len - processed);
		client->len -= processed;
		client->buffer[client->len] = '\0';
	}

	return 0;
}

static int ipc_handle_connection(int fd, uint32_t mask, void *data) {
	struct mint_server *server = data;
	int client_fd = accept4(fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
	if (client_fd < 0) {
		return 0;
	}

	struct mint_ipc_client *client = calloc(1, sizeof(*client));
	if (!client) {
		close(client_fd);
		return 0;
	}

	client->server = server;
	client->fd = client_fd;
	wl_list_insert(&server->ipc_clients, &client->link);

	struct wl_event_loop *loop = wl_display_get_event_loop(server->wl_display);
	client->event_source = wl_event_loop_add_fd(
		loop, client_fd, WL_EVENT_READABLE, ipc_handle_client_data, client);

	return 0;
}

static int ipc_init(struct mint_server *server) {
	wl_list_init(&server->ipc_clients);

	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir || runtime_dir[0] == '\0') {
		runtime_dir = "/tmp";
	}

	snprintf(server->ipc_socket_path, sizeof(server->ipc_socket_path),
		"%s/mint-ipc.sock", runtime_dir);

	unlink(server->ipc_socket_path);

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
	if (fd < 0) {
		wlr_log(WLR_ERROR, "Failed to create IPC socket");
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlen(server->ipc_socket_path) >= sizeof(addr.sun_path)) {
		wlr_log(WLR_ERROR, "IPC socket path too long");
		close(fd);
		return -1;
	}
	memcpy(addr.sun_path, server->ipc_socket_path, strlen(server->ipc_socket_path) + 1);

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "Failed to bind IPC socket to %s", server->ipc_socket_path);
		close(fd);
		return -1;
	}

	if (listen(fd, 16) < 0) {
		wlr_log(WLR_ERROR, "Failed to listen on IPC socket");
		close(fd);
		return -1;
	}

	chmod(server->ipc_socket_path, 0600);
	setenv("MINT_IPC_SOCKET", server->ipc_socket_path, 1);
	setenv("TINYWL_IPC_SOCKET", server->ipc_socket_path, 1);
	wlr_log(WLR_INFO, "IPC socket listening on %s", server->ipc_socket_path);

	struct wl_event_loop *loop = wl_display_get_event_loop(server->wl_display);
	server->ipc_event_source = wl_event_loop_add_fd(
		loop, fd, WL_EVENT_READABLE, ipc_handle_connection, server);
	server->ipc_fd = fd;
	return 0;
}

static void ipc_finish(struct mint_server *server) {
	struct mint_ipc_client *client, *tmp;
	wl_list_for_each_safe(client, tmp, &server->ipc_clients, link) {
		ipc_client_destroy(client);
	}
	if (server->ipc_event_source) {
		wl_event_source_remove(server->ipc_event_source);
	}
	if (server->ipc_fd >= 0) {
		close(server->ipc_fd);
	}
	if (server->ipc_socket_path[0] != '\0') {
		unlink(server->ipc_socket_path);
	}
}

#if CONFIG_SESSION_LOCK
/* Session Lock (ext-session-lock-v1) */
static void update_lock_bg(struct mint_server *server) {
	if (!server->lock_bg) return;
	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, NULL, &box);
	wlr_scene_node_set_position(&server->lock_bg->node, box.x, box.y);
	wlr_scene_rect_set_size(server->lock_bg, box.width, box.height);
	wlr_scene_node_lower_to_bottom(&server->lock_bg->node);
}

static void session_lock_surface_handle_destroy(struct wl_listener *listener, void *data) {
	struct mint_session_lock_surface *surface =
		wl_container_of(listener, surface, destroy);

	wl_list_remove(&surface->destroy.link);
	wl_list_remove(&surface->link);

	if (surface->scene_tree) {
		wlr_scene_node_destroy(&surface->scene_tree->node);
		surface->scene_tree = NULL;
	}

	struct mint_server *server = surface->server;
	free(surface);

	if (server->locked && !wl_list_empty(&server->lock_surfaces)) {
		struct mint_session_lock_surface *first =
			wl_container_of(server->lock_surfaces.next, first, link);
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
		if (keyboard != NULL) {
			wlr_seat_keyboard_notify_enter(server->seat, first->lock_surface->surface,
				keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
		} else {
			wlr_seat_keyboard_notify_enter(server->seat, first->lock_surface->surface,
				NULL, 0, NULL);
		}
	} else if (!server->locked) {
		if (server->focused_toplevel) {
			focus_toplevel(server, server->focused_toplevel);
		}
	} else {
		wlr_seat_keyboard_notify_clear_focus(server->seat);
	}
}

static void session_lock_handle_new_surface(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, session_lock_new_surface);
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct mint_output *output = lock_surface->output ? lock_surface->output->data : NULL;

	if (!output) {
		return;
	}

	struct mint_session_lock_surface *surface = calloc(1, sizeof(*surface));
	if (!surface) {
		return;
	}

	surface->server = server;
	surface->output = output;
	surface->lock_surface = lock_surface;
	lock_surface->data = surface;

	surface->scene_tree = wlr_scene_subsurface_tree_create(
		server->scene_tree_lock, lock_surface->surface);

	struct wlr_box box;
	wlr_output_layout_get_box(server->output_layout, output->wlr_output, &box);
	wlr_scene_node_set_position(&surface->scene_tree->node, box.x, box.y);

	wlr_session_lock_surface_v1_configure(lock_surface, box.width, box.height);

	surface->destroy.notify = session_lock_surface_handle_destroy;
	wl_signal_add(&lock_surface->events.destroy, &surface->destroy);

	wl_list_insert(&server->lock_surfaces, &surface->link);

	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
	if (keyboard != NULL) {
		wlr_seat_keyboard_notify_enter(server->seat, lock_surface->surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(server->seat, lock_surface->surface,
			NULL, 0, NULL);
	}
}

static void session_lock_handle_unlock(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, session_lock_unlock);

	server->locked = false;
	server->session_lock = NULL;

	wl_list_remove(&server->session_lock_new_surface.link);
	wl_list_remove(&server->session_lock_unlock.link);
	wl_list_remove(&server->session_lock_destroy.link);

	if (server->lock_bg) {
		wlr_scene_node_set_enabled(&server->lock_bg->node, false);
	}

	struct mint_session_lock_surface *surface, *tmp;
	wl_list_for_each_safe(surface, tmp, &server->lock_surfaces, link) {
		wl_list_remove(&surface->destroy.link);
		wl_list_remove(&surface->link);
		if (surface->scene_tree) {
			wlr_scene_node_destroy(&surface->scene_tree->node);
		}
		free(surface);
	}

	wlr_seat_keyboard_notify_clear_focus(server->seat);
	if (server->focused_toplevel) {
		focus_toplevel(server, server->focused_toplevel);
	}
}

static void session_lock_handle_destroy(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, session_lock_destroy);

	wl_list_remove(&server->session_lock_new_surface.link);
	wl_list_remove(&server->session_lock_unlock.link);
	wl_list_remove(&server->session_lock_destroy.link);

	server->session_lock = NULL;

	struct mint_session_lock_surface *surface, *tmp;
	wl_list_for_each_safe(surface, tmp, &server->lock_surfaces, link) {
		wl_list_remove(&surface->destroy.link);
		wl_list_remove(&surface->link);
		if (surface->scene_tree) {
			wlr_scene_node_destroy(&surface->scene_tree->node);
		}
		free(surface);
	}

	/* If client died while locked without unlocking, session must remain locked */
	if (server->locked) {
		wlr_seat_keyboard_notify_clear_focus(server->seat);
		update_lock_bg(server);
		if (server->lock_bg) {
			wlr_scene_node_set_enabled(&server->lock_bg->node, true);
		}
	}
}

static void server_new_session_lock(struct wl_listener *listener, void *data) {
	struct mint_server *server =
		wl_container_of(listener, server, new_session_lock);
	struct wlr_session_lock_v1 *lock = data;

	if (server->session_lock != NULL) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	server->session_lock = lock;
	server->locked = true;

	update_lock_bg(server);
	if (server->lock_bg) {
		wlr_scene_node_set_enabled(&server->lock_bg->node, true);
	}

	wlr_seat_keyboard_notify_clear_focus(server->seat);

	server->session_lock_new_surface.notify = session_lock_handle_new_surface;
	wl_signal_add(&lock->events.new_surface, &server->session_lock_new_surface);

	server->session_lock_unlock.notify = session_lock_handle_unlock;
	wl_signal_add(&lock->events.unlock, &server->session_lock_unlock);

	server->session_lock_destroy.notify = session_lock_handle_destroy;
	wl_signal_add(&lock->events.destroy, &server->session_lock_destroy);

	wlr_session_lock_v1_send_locked(lock);
}

#else
static inline void update_lock_bg(struct mint_server *server) {
	(void)server;
}
#endif
static struct mint_server *g_server = NULL;

static void handle_sigchld(int signo) {
	(void)signo;
	siginfo_t in;
	while (!waitid(P_ALL, 0, &in, WEXITED | WNOHANG | WNOWAIT) && in.si_pid) {
#if CONFIG_XWAYLAND
		if (g_server && g_server->xwayland && g_server->xwayland->server &&
				in.si_pid == g_server->xwayland->server->pid) {
			break;
		}
#endif
		waitpid(in.si_pid, NULL, 0);
	}
}

#if CONFIG_SCREENCOPY
static const char *const screencopy_allowed_clients[] = {
	"grim",
	"xdg-desktop-portal-wlr",
	"hyprlock",
	"swaylock",
};

static bool match_binary_name(const char *path, const char *target) {
	if (!path || !target) {
		return false;
	}

	const char *base = strrchr(path, '/');
	if (base) {
		base++;
	} else {
		base = path;
	}

	/* Strip leading dot (e.g. Nixpkgs wrapper .foo-wrapped) */
	if (base[0] == '.') {
		base++;
	}

	/* Make a local copy to strip suffixes */
	char clean[256];
	strncpy(clean, base, sizeof(clean) - 1);
	clean[sizeof(clean) - 1] = '\0';

	/* Strip " (deleted)" suffix if present */
	char *del = strstr(clean, " (deleted)");
	if (del) {
		*del = '\0';
	}

	/* Strip "-wrapped" suffix if present (Nixpkgs wrapper) */
	size_t clen = strlen(clean);
	static const char wrapped_suffix[] = "-wrapped";
	size_t wlen = sizeof(wrapped_suffix) - 1;
	if (clen > wlen && strcmp(clean + clen - wlen, wrapped_suffix) == 0) {
		clean[clen - wlen] = '\0';
	}

	return strcmp(clean, target) == 0;
}

static bool is_authorized_screencopy_client(const struct wl_client *client) {
	pid_t pid = 0;
	uid_t uid = 0;
	gid_t gid = 0;
	wl_client_get_credentials(client, &pid, &uid, &gid);

	if (pid <= 0) {
		return false;
	}

	/* 1. Check /proc/<pid>/exe target */
	char proc_path[64];
	snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", pid);

	char exe_path[PATH_MAX];
	ssize_t len = readlink(proc_path, exe_path, sizeof(exe_path) - 1);
	if (len > 0) {
		exe_path[len] = '\0';
		for (size_t i = 0; i < sizeof(screencopy_allowed_clients) / sizeof(screencopy_allowed_clients[0]); i++) {
			if (match_binary_name(exe_path, screencopy_allowed_clients[i])) {
				return true;
			}
		}
	}

	/* 2. Check /proc/<pid>/cmdline (argv[0]) for wrapper scripts */
	snprintf(proc_path, sizeof(proc_path), "/proc/%d/cmdline", pid);
	FILE *f = fopen(proc_path, "r");
	if (f) {
		char cmdline[PATH_MAX];
		size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
		fclose(f);
		if (n > 0) {
			cmdline[n] = '\0';
			for (size_t i = 0; i < sizeof(screencopy_allowed_clients) / sizeof(screencopy_allowed_clients[0]); i++) {
				if (match_binary_name(cmdline, screencopy_allowed_clients[i])) {
					return true;
				}
			}
		}
	}

	return false;
}

static bool server_global_filter(const struct wl_client *client,
		const struct wl_global *global, void *data) {
	struct mint_server *server = data;

	if (server->screencopy_mgr && global == server->screencopy_mgr->global) {
		return is_authorized_screencopy_client(client);
	}

	return true;
}

#endif

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_INFO, NULL);
	char *startup_cmd = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:h")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("Usage: %s [-s startup command]\n", argv[0]);
			return 0;
		}
	}

	struct mint_server server = {0};
	server.current_workspace = 1;
	server.prev_workspace = 1;
	server.mfact = DEFAULT_MFACT;
#if CONFIG_GAPS
	server.gaps = DEFAULT_GAPS;
	server.gaps_enabled = true;
	server.smart_gaps = DEFAULT_SMART_GAPS;
#endif
#if CONFIG_SWALLOWING
	server.auto_swallow = true;
#endif
	server.ipc_fd = -1;
	g_server = &server;

	/* Reap child processes without waiting on Xwayland */
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_sigchld;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sa, NULL);

	server.wl_display = wl_display_create();
	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);
	if (server.backend == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_backend");
		return 1;
	}

	server.renderer = wlr_renderer_autocreate(server.backend);
	if (server.renderer == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_renderer");
		return 1;
	}

	wlr_renderer_init_wl_display(server.renderer, server.wl_display);

	server.allocator = wlr_allocator_autocreate(server.backend, server.renderer);
	if (server.allocator == NULL) {
		wlr_log(WLR_ERROR, "failed to create wlr_allocator");
		return 1;
	}

	server.compositor = wlr_compositor_create(server.wl_display, 5, server.renderer);
	wlr_subcompositor_create(server.wl_display);
	wlr_data_device_manager_create(server.wl_display);

#if CONFIG_DATA_CONTROL
	wlr_data_control_manager_v1_create(server.wl_display);
	wlr_ext_data_control_manager_v1_create(server.wl_display, 1);
#endif

#if CONFIG_PRIMARY_SELECTION
	wlr_primary_selection_v1_device_manager_create(server.wl_display);
#endif

#if CONFIG_VIEWPORTER
	wlr_viewporter_create(server.wl_display);
#endif

#if CONFIG_SCREENCOPY
	server.screencopy_mgr = wlr_screencopy_manager_v1_create(server.wl_display);
	wl_display_set_global_filter(server.wl_display, server_global_filter, &server);
#endif

#if CONFIG_XWAYLAND
	server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, true);
	if (server.xwayland) {
		server.xwayland_ready.notify = handle_xwayland_ready;
		wl_signal_add(&server.xwayland->events.ready, &server.xwayland_ready);
		server.new_xwayland_surface.notify = handle_new_xwayland_surface;
		wl_signal_add(&server.xwayland->events.new_surface, &server.new_xwayland_surface);
		if (server.xwayland->display_name) {
			setenv("DISPLAY", server.xwayland->display_name, true);
		}
	} else {
		wlr_log(WLR_ERROR, "failed to start Xwayland");
	}
#endif

	server.output_layout = wlr_output_layout_create(server.wl_display);

#if CONFIG_XDG_OUTPUT
	server.xdg_output_manager =
		wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout);
#endif

	wl_list_init(&server.outputs);
	server.new_output.notify = server_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.scene = wlr_scene_create();
	server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

	/* Layered scene tree hierarchy */
	server.scene_tree_background = wlr_scene_tree_create(&server.scene->tree);
	server.scene_tree_bottom = wlr_scene_tree_create(&server.scene->tree);
	server.scene_tree_windows = wlr_scene_tree_create(&server.scene->tree);
	server.scene_tree_top = wlr_scene_tree_create(&server.scene->tree);
	server.scene_tree_fullscreen = wlr_scene_tree_create(&server.scene->tree);
	server.scene_tree_overlay = wlr_scene_tree_create(&server.scene->tree);
#if CONFIG_SESSION_LOCK
	server.scene_tree_lock = wlr_scene_tree_create(&server.scene->tree);
	static const float lock_black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	server.lock_bg = wlr_scene_rect_create(server.scene_tree_lock, 0, 0, lock_black);
	wlr_scene_node_set_enabled(&server.lock_bg->node, false);
#endif

	wl_list_init(&server.toplevels);
	server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 3);
	server.new_xdg_toplevel.notify = server_new_xdg_toplevel;
	wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
	server.new_xdg_popup.notify = server_new_xdg_popup;
	wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

#if CONFIG_XDG_DECORATION
	server.xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(server.wl_display);
	if (server.xdg_decoration_mgr) {
		server.new_xdg_decoration.notify = server_new_xdg_decoration;
		wl_signal_add(&server.xdg_decoration_mgr->events.new_toplevel_decoration,
			&server.new_xdg_decoration);
	}

	server.server_decoration_mgr = wlr_server_decoration_manager_create(server.wl_display);
	if (server.server_decoration_mgr) {
		wlr_server_decoration_manager_set_default_mode(server.server_decoration_mgr,
			WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
	}
#endif

#if CONFIG_LAYER_SHELL
	server.layer_shell = wlr_layer_shell_v1_create(server.wl_display, 4);
	if (server.layer_shell) {
		server.new_layer_shell_surface.notify = server_new_layer_shell_surface;
		wl_signal_add(&server.layer_shell->events.new_surface, &server.new_layer_shell_surface);
	}
#endif

#if CONFIG_SESSION_LOCK
	wl_list_init(&server.lock_surfaces);
	server.session_lock_mgr = wlr_session_lock_manager_v1_create(server.wl_display);
	if (server.session_lock_mgr) {
		server.new_session_lock.notify = server_new_session_lock;
		wl_signal_add(&server.session_lock_mgr->events.new_lock, &server.new_session_lock);
	}
#endif

	server.cursor = wlr_cursor_create();
	wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

	server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

	server.cursor_motion.notify = server_cursor_motion;
	wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);
	server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
	wl_signal_add(&server.cursor->events.motion_absolute,
			&server.cursor_motion_absolute);
	server.cursor_button.notify = server_cursor_button;
	wl_signal_add(&server.cursor->events.button, &server.cursor_button);
	server.cursor_axis.notify = server_cursor_axis;
	wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);
	server.cursor_frame.notify = server_cursor_frame;
	wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

	wl_list_init(&server.keyboards);
	server.new_input.notify = server_new_input;
	wl_signal_add(&server.backend->events.new_input, &server.new_input);
	server.seat = wlr_seat_create(server.wl_display, "seat0");
#if CONFIG_XWAYLAND
	if (server.xwayland) {
		wlr_xwayland_set_seat(server.xwayland, server.seat);
	}
#endif
	server.request_cursor.notify = seat_request_cursor;
	wl_signal_add(&server.seat->events.request_set_cursor,
			&server.request_cursor);
	server.pointer_focus_change.notify = seat_pointer_focus_change;
	wl_signal_add(&server.seat->pointer_state.events.focus_change,
			&server.pointer_focus_change);
	server.request_set_selection.notify = seat_request_set_selection;
	wl_signal_add(&server.seat->events.request_set_selection,
			&server.request_set_selection);
#if CONFIG_PRIMARY_SELECTION
	server.request_set_primary_selection.notify = seat_request_set_primary_selection;
	wl_signal_add(&server.seat->events.request_set_primary_selection,
			&server.request_set_primary_selection);
#endif

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wlr_backend_destroy(server.backend);
		return 1;
	}

	if (ipc_init(&server) < 0) {
		wlr_log(WLR_ERROR, "failed to initialize IPC");
	}

	if (!wlr_backend_start(server.backend)) {
		ipc_finish(&server);
		wlr_backend_destroy(server.backend);
		wl_display_destroy(server.wl_display);
		return 1;
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd) {
		pid_t pid = fork();
		if (pid == 0) {
			setsid();
			execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (char *)NULL);
			_exit(1);
		} else if (pid < 0) {
			wlr_log(WLR_ERROR, "failed to fork startup command");
		}
	}

	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s", socket);
	wl_display_run(server.wl_display);

	ipc_finish(&server);

#if CONFIG_XWAYLAND
	if (server.xwayland) {
		wl_list_remove(&server.xwayland_ready.link);
		wl_list_remove(&server.new_xwayland_surface.link);
		wlr_xwayland_destroy(server.xwayland);
		server.xwayland = NULL;
	}
#endif

	wl_display_destroy_clients(server.wl_display);

	wl_list_remove(&server.new_xdg_toplevel.link);
	wl_list_remove(&server.new_xdg_popup.link);
#if CONFIG_XDG_DECORATION
	if (server.xdg_decoration_mgr) {
		wl_list_remove(&server.new_xdg_decoration.link);
	}
#endif
#if CONFIG_LAYER_SHELL
	if (server.layer_shell) {
		wl_list_remove(&server.new_layer_shell_surface.link);
	}
#endif
#if CONFIG_SESSION_LOCK
	if (server.session_lock_mgr) {
		wl_list_remove(&server.new_session_lock.link);
	}
#endif

	wl_list_remove(&server.cursor_motion.link);
	wl_list_remove(&server.cursor_motion_absolute.link);
	wl_list_remove(&server.cursor_button.link);
	wl_list_remove(&server.cursor_axis.link);
	wl_list_remove(&server.cursor_frame.link);

	wl_list_remove(&server.new_input.link);
	wl_list_remove(&server.request_cursor.link);
	wl_list_remove(&server.pointer_focus_change.link);
	wl_list_remove(&server.request_set_selection.link);
#if CONFIG_PRIMARY_SELECTION
	wl_list_remove(&server.request_set_primary_selection.link);
#endif

	wl_list_remove(&server.new_output.link);

	wlr_scene_node_destroy(&server.scene->tree.node);
	wlr_xcursor_manager_destroy(server.cursor_mgr);
	wlr_cursor_destroy(server.cursor);
	wlr_allocator_destroy(server.allocator);
	wlr_renderer_destroy(server.renderer);
	wlr_backend_destroy(server.backend);
	wl_display_destroy(server.wl_display);
	return 0;
}
