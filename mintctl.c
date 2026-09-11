#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static void print_usage(const char *prog) {
	fprintf(stderr, "Usage: %s <command...>\n", prog);
	fprintf(stderr, "Commands:\n");
	fprintf(stderr, "  sh <command>              Run shell command in background\n");
	fprintf(stderr, "  run <command>             Alias for sh\n");
	fprintf(stderr, "  workspace <1-9|next|prev|back> Switch workspace\n");
	fprintf(stderr, "  moveto <1-9|back>         Move focused window to workspace\n");
	fprintf(stderr, "  close                     Close focused window\n");
	fprintf(stderr, "  focus <next|prev|master>  Change window focus\n");
	fprintf(stderr, "  swap                      Swap focused window with master\n");
	fprintf(stderr, "  fullscreen                Toggle fullscreen for focused window\n");
	fprintf(stderr, "  toggle_swallow            Toggle window swallow\n");
	fprintf(stderr, "  get_swallow               Print swallowing state (0 or 1)\n");
	fprintf(stderr, "  toggle_auto_swallow       Toggle auto-swallow on/off\n");
	fprintf(stderr, "  auto_swallow <on|off|toggle> Control auto-swallow\n");
	fprintf(stderr, "  toggle_gaps               Toggle window gaps on/off\n");
	fprintf(stderr, "  gaps <0-100|+N|-N|on|off> Control gap size or state\n");
	fprintf(stderr, "  smart_gaps <on|off|toggle> Control smart gaps (no gap for 1 window)\n");
	fprintf(stderr, "  mfact <0.1-0.9|+/-0.05>   Change master area factor\n");
	fprintf(stderr, "  get_workspace             Print active workspace number\n");
	fprintf(stderr, "  get_workspaces            Print workspace list with [active]\n");
	fprintf(stderr, "  get_title                 Print active window title\n");
	fprintf(stderr, "  subscribe                 Stream live state events\n");
	fprintf(stderr, "  status                    Print JSON status of compositor\n");
	fprintf(stderr, "  exit                      Exit the compositor\n");
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		print_usage(argv[0]);
		return 1;
	}

	const char *sock_path = getenv("MINT_IPC_SOCKET");
	if (!sock_path || sock_path[0] == '\0') {
		sock_path = getenv("TINYWL_IPC_SOCKET");
	}
	char default_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	if (!sock_path || sock_path[0] == '\0') {
		const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
		if (!runtime_dir) {
			runtime_dir = "/tmp";
		}
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
		fprintf(stderr, "Error: socket path is too long\n");
		close(fd);
		return 1;
	}
	memcpy(addr.sun_path, sock_path, strlen(sock_path) + 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("connect");
		fprintf(stderr, "Error: Could not connect to MinT IPC socket at %s\n", sock_path);
		close(fd);
		return 1;
	}

	char cmd[2048] = {0};
	size_t off = 0;
	for (int i = 1; i < argc; i++) {
		if (i > 1 && off + 1 < sizeof(cmd)) {
			cmd[off++] = ' ';
		}
		int n = snprintf(cmd + off, sizeof(cmd) - off, "%s", argv[i]);
		if (n > 0) {
			off += (size_t)n;
			if (off >= sizeof(cmd) - 2) {
				off = sizeof(cmd) - 2;
				break;
			}
		}
	}
	if (off + 1 < sizeof(cmd)) {
		cmd[off++] = '\n';
		cmd[off] = '\0';
	}

	size_t len = strlen(cmd);
	if (write(fd, cmd, len) != (ssize_t)len) {
		perror("write");
		close(fd);
		return 1;
	}

	bool is_subscribe = (strcmp(argv[1], "subscribe") == 0);
	if (is_subscribe) {
		char response[1024];
		ssize_t n;
		while ((n = read(fd, response, sizeof(response) - 1)) > 0) {
			response[n] = '\0';
			fputs(response, stdout);
			fflush(stdout);
		}
	} else {
		char response[2048];
		ssize_t n = read(fd, response, sizeof(response) - 1);
		if (n > 0) {
			response[n] = '\0';
			fputs(response, stdout);
		}
	}

	close(fd);
	return 0;
}
