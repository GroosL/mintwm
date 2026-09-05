#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <xkbcommon/xkbcommon.h>

struct Binding {
	int mode;
	uint32_t mod;
	xkb_keysym_t keysym;
	const char *cmd;
	int next_mode;
};

#include "config.h"

static volatile sig_atomic_t running = 1;

static void handle_sig(int sig) {
	(void)sig;
	running = 0;
}

static bool keysym_matches(xkb_keysym_t target, xkb_keysym_t sym, xkb_keysym_t raw_sym) {
	if (sym == target || raw_sym == target) {
		return true;
	}
	if (xkb_keysym_to_lower(sym) == xkb_keysym_to_lower(target) ||
	    xkb_keysym_to_lower(raw_sym) == xkb_keysym_to_lower(target)) {
		return true;
	}
	if (target >= XKB_KEY_1 && target <= XKB_KEY_9) {
		static const xkb_keysym_t shifted_numbers[] = {
			XKB_KEY_exclam,      /* 1 */
			XKB_KEY_at,          /* 2 */
			XKB_KEY_numbersign,  /* 3 */
			XKB_KEY_dollar,      /* 4 */
			XKB_KEY_percent,     /* 5 */
			XKB_KEY_asciicircum, /* 6 */
			XKB_KEY_ampersand,   /* 7 */
			XKB_KEY_asterisk,    /* 8 */
			XKB_KEY_parenleft,   /* 9 */
		};
		size_t idx = target - XKB_KEY_1;
		if (sym == shifted_numbers[idx]) {
			return true;
		}
	}
	return false;
}

static bool mod_matches(int current_mode, uint32_t target_mod, uint32_t current_mods) {
	if (current_mode != MODE_NORMAL && target_mod == 0) {
		/* In chord mode, allow key whether modifier was released or is still held */
		return (current_mods == 0 || current_mods == MODKEY);
	}
	return (target_mod == current_mods);
}

static const char *process_key(int *mode, time_t *chord_start,
		uint32_t mods, xkb_keysym_t sym, xkb_keysym_t raw_sym,
		uint32_t state) {
	time_t now = time(NULL);

	/* Check chord timeout */
	if (*mode != MODE_NORMAL && (now - *chord_start) > CHORD_TIMEOUT_SECS) {
		*mode = MODE_NORMAL;
	}

	/* On key release: pass */
	if (state == 0) {
		return "PASS";
	}

	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings[0]); i++) {
		const struct Binding *b = &bindings[i];
		if (b->mode == *mode && mod_matches(*mode, b->mod, mods) && keysym_matches(b->keysym, sym, raw_sym)) {
			*mode = b->next_mode;
			if (*mode != MODE_NORMAL) {
				*chord_start = now;
			}
			return b->cmd ? b->cmd : "SWALLOW";
		}
	}

	/* If in chord mode, any unmapped key cancels chord mode and is swallowed */
	if (*mode != MODE_NORMAL) {
		*mode = MODE_NORMAL;
		return "SWALLOW";
	}

	return "PASS";
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	signal(SIGINT, handle_sig);
	signal(SIGTERM, handle_sig);
	signal(SIGPIPE, SIG_IGN);

	const char *sock_path = getenv("MINT_IPC_SOCKET");
	if (!sock_path || sock_path[0] == '\0') {
		sock_path = getenv("TINYWL_IPC_SOCKET");
	}
	char default_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	if (!sock_path || sock_path[0] == '\0') {
		const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
		if (!runtime_dir) runtime_dir = "/tmp";
		snprintf(default_path, sizeof(default_path), "%s/mint-ipc.sock", runtime_dir);
		sock_path = default_path;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlen(sock_path) >= sizeof(addr.sun_path)) {
		fprintf(stderr, "Error: socket path too long\n");
		close(fd);
		return 1;
	}
	memcpy(addr.sun_path, sock_path, strlen(sock_path) + 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		fprintf(stderr, "Could not connect to MinT IPC socket at %s\n", sock_path);
		close(fd);
		return 1;
	}

	/* Request key grab */
	const char *req = "grab_keys\n";
	if (write(fd, req, strlen(req)) <= 0) {
		perror("write grab_keys");
		close(fd);
		return 1;
	}

	char buf[1024];
	char line[512];
	size_t line_len = 0;
	int mode = MODE_NORMAL;
	time_t chord_start = 0;

	printf("mint-keysd daemon connected and active.\n");

	while (running) {
		ssize_t n = read(fd, buf, sizeof(buf));
		if (n <= 0) {
			break;
		}

		for (ssize_t i = 0; i < n; i++) {
			char c = buf[i];
			if (c != '\n') {
				if (line_len < sizeof(line) - 1) {
					line[line_len++] = c;
				}
				continue;
			}

			line[line_len] = '\0';
			line_len = 0;

			/* Check for KEY event: KEY <mods> <sym> <raw_sym> <state> */
			uint32_t mods = 0, state = 0;
			xkb_keysym_t sym = XKB_KEY_NoSymbol, raw_sym = XKB_KEY_NoSymbol;

			int parsed = sscanf(line, "KEY %u %u %u %u", &mods, &sym, &raw_sym, &state);
			if (parsed == 4) {
				const char *reply = process_key(&mode, &chord_start,
					mods, sym, raw_sym, state);

				char out[256];
				int out_len = snprintf(out, sizeof(out), "%s\n", reply);
				if (write(fd, out, out_len) <= 0) {
					running = 0;
					break;
				}
			} else if (parsed == 3) {
				state = raw_sym;
				raw_sym = sym;
				const char *reply = process_key(&mode, &chord_start,
					mods, sym, raw_sym, state);

				char out[256];
				int out_len = snprintf(out, sizeof(out), "%s\n", reply);
				if (write(fd, out, out_len) <= 0) {
					running = 0;
					break;
				}
			}
		}
	}

	/* Clean exit */
	const char *ungrab = "ungrab_keys\n";
	if (write(fd, ungrab, strlen(ungrab)) < 0) {
		/* ignore */
	}
	close(fd);
	printf("mint-keysd daemon stopped.\n");
	return 0;
}
