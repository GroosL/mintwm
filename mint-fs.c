#define FUSE_USE_VERSION 31
#define _GNU_SOURCE

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

static char last_ctl_reply[1024] = "OK MinTwm FUSE ready\n";

/* IPC connection helper */
static int ipc_connect(void) {
	const char *sock_path = getenv("MINT_IPC_SOCKET");
	if (!sock_path || sock_path[0] == '\0') {
		sock_path = getenv("TINYWL_IPC_SOCKET");
	}
	char default_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
	if (!sock_path || sock_path[0] == '\0') {
		const char *rt = getenv("XDG_RUNTIME_DIR");
		if (!rt) rt = "/tmp";
		snprintf(default_path, sizeof(default_path), "%s/mint-ipc.sock", rt);
		sock_path = default_path;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (strlen(sock_path) >= sizeof(addr.sun_path)) {
		close(fd);
		return -1;
	}
	memcpy(addr.sun_path, sock_path, strlen(sock_path) + 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

static int ipc_query(const char *cmd, char *resp, size_t resp_size) {
	int fd = ipc_connect();
	if (fd < 0) {
		if (resp && resp_size > 0) {
			snprintf(resp, resp_size, "ERROR could not connect to MinTwm IPC\n");
		}
		return -1;
	}

	size_t len = strlen(cmd);
	if (write(fd, cmd, len) != (ssize_t)len) {
		close(fd);
		if (resp && resp_size > 0) {
			snprintf(resp, resp_size, "ERROR write to IPC failed\n");
		}
		return -1;
	}

	if (resp && resp_size > 0) {
		ssize_t n = read(fd, resp, resp_size - 1);
		if (n >= 0) {
			resp[n] = '\0';
		} else {
			resp[0] = '\0';
		}
	}

	close(fd);
	return 0;
}

/* Helper to strip trailing newlines and whitespace for commands */
static void sanitize_input(char *dst, const char *src, size_t max) {
	size_t i = 0;
	while (i < max - 1 && src[i] != '\0') {
		if (src[i] == '\r' || src[i] == '\n') {
			break;
		}
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';

	/* Trim trailing whitespace */
	while (i > 0 && (dst[i - 1] == ' ' || dst[i - 1] == '\t')) {
		dst[--i] = '\0';
	}
}

/* Query content for virtual file paths */
static int get_file_content(const char *path, char *buf, size_t max) {
	if (strcmp(path, "/current_workspace") == 0) {
		return ipc_query("get_workspace\n", buf, max);
	} else if (strcmp(path, "/title") == 0 || strcmp(path, "/windows/active/title") == 0) {
		return ipc_query("get_title\n", buf, max);
	} else if (strcmp(path, "/status") == 0) {
		return ipc_query("status\n", buf, max);
	} else if (strcmp(path, "/workspaces") == 0) {
		return ipc_query("get_workspaces\n", buf, max);
	} else if (strcmp(path, "/mfact") == 0) {
		char reply[128] = {0};
		if (ipc_query("mfact\n", reply, sizeof(reply)) == 0) {
			if (strncmp(reply, "OK mfact ", 9) == 0) {
				snprintf(buf, max, "%s", reply + 9);
			} else {
				snprintf(buf, max, "%s", reply);
			}
			return 0;
		}
		return -1;
	} else if (strcmp(path, "/ctl") == 0) {
		snprintf(buf, max, "%s", last_ctl_reply);
		return 0;
	} else if (strcmp(path, "/swallow") == 0 || strcmp(path, "/windows/active/swallow") == 0) {
		return ipc_query("get_swallow\n", buf, max);
	} else if (strcmp(path, "/auto_swallow") == 0) {
		return ipc_query("auto_swallow\n", buf, max);
	} else if (strcmp(path, "/windows/active/workspace") == 0) {
		return ipc_query("get_workspace\n", buf, max);
	}
	return -ENOENT;
}

static int mint_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
	(void)fi;
	memset(stbuf, 0, sizeof(struct stat));

	if (strcmp(path, "/") == 0 || strcmp(path, "/windows") == 0 ||
	    strcmp(path, "/windows/active") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	stbuf->st_nlink = 1;

	if (strcmp(path, "/ctl") == 0 ||
	    strcmp(path, "/current_workspace") == 0 ||
	    strcmp(path, "/mfact") == 0 ||
	    strcmp(path, "/swallow") == 0 ||
	    strcmp(path, "/auto_swallow") == 0 ||
	    strcmp(path, "/windows/active/swallow") == 0 ||
	    strcmp(path, "/windows/active/workspace") == 0) {
		stbuf->st_mode = S_IFREG | 0666;
		char content[1024];
		if (get_file_content(path, content, sizeof(content)) == 0) {
			stbuf->st_size = strlen(content);
		} else {
			stbuf->st_size = 64;
		}
		return 0;
	}

	if (strcmp(path, "/title") == 0 ||
	    strcmp(path, "/status") == 0 ||
	    strcmp(path, "/workspaces") == 0 ||
	    strcmp(path, "/windows/active/title") == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		char content[1024];
		if (get_file_content(path, content, sizeof(content)) == 0) {
			stbuf->st_size = strlen(content);
		} else {
			stbuf->st_size = 256;
		}
		return 0;
	}

	if (strcmp(path, "/focus") == 0 ||
	    strcmp(path, "/windows/active/ctl") == 0) {
		stbuf->st_mode = S_IFREG | 0222;
		stbuf->st_size = 0;
		return 0;
	}

	return -ENOENT;
}

static int mint_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
	(void)offset;
	(void)fi;
	(void)flags;

	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		filler(buf, "ctl", NULL, 0, 0);
		filler(buf, "current_workspace", NULL, 0, 0);
		filler(buf, "title", NULL, 0, 0);
		filler(buf, "status", NULL, 0, 0);
		filler(buf, "workspaces", NULL, 0, 0);
		filler(buf, "mfact", NULL, 0, 0);
		filler(buf, "focus", NULL, 0, 0);
		filler(buf, "swallow", NULL, 0, 0);
		filler(buf, "auto_swallow", NULL, 0, 0);
		filler(buf, "windows", NULL, 0, 0);
		return 0;
	}

	if (strcmp(path, "/windows") == 0) {
		filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		filler(buf, "active", NULL, 0, 0);
		return 0;
	}

	if (strcmp(path, "/windows/active") == 0) {
		filler(buf, ".", NULL, 0, 0);
		filler(buf, "..", NULL, 0, 0);
		filler(buf, "title", NULL, 0, 0);
		filler(buf, "ctl", NULL, 0, 0);
		filler(buf, "workspace", NULL, 0, 0);
		filler(buf, "swallow", NULL, 0, 0);
		return 0;
	}

	return -ENOENT;
}

static int mint_open(const char *path, struct fuse_file_info *fi) {
	/* Direct I/O disables page cache so file reads always reflect live state */
	fi->direct_io = 1;

	if (strcmp(path, "/ctl") == 0 ||
	    strcmp(path, "/current_workspace") == 0 ||
	    strcmp(path, "/title") == 0 ||
	    strcmp(path, "/status") == 0 ||
	    strcmp(path, "/workspaces") == 0 ||
	    strcmp(path, "/mfact") == 0 ||
	    strcmp(path, "/focus") == 0 ||
	    strcmp(path, "/swallow") == 0 ||
	    strcmp(path, "/auto_swallow") == 0 ||
	    strcmp(path, "/windows/active/title") == 0 ||
	    strcmp(path, "/windows/active/ctl") == 0 ||
	    strcmp(path, "/windows/active/workspace") == 0 ||
	    strcmp(path, "/windows/active/swallow") == 0) {
		return 0;
	}

	return -ENOENT;
}

static int mint_truncate(const char *path, off_t size, struct fuse_file_info *fi) {
	(void)path;
	(void)size;
	(void)fi;
	return 0;
}

static int mint_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi) {
	(void)fi;
	char content[2048] = {0};

	int res = get_file_content(path, content, sizeof(content));
	if (res < 0) {
		return res;
	}

	size_t len = strlen(content);
	if (offset >= (off_t)len) {
		return 0;
	}

	if (offset + size > len) {
		size = len - offset;
	}

	memcpy(buf, content + offset, size);
	return (int)size;
}

static int mint_write(const char *path, const char *buf, size_t size,
		off_t offset, struct fuse_file_info *fi) {
	(void)offset;
	(void)fi;

	char clean[512];
	sanitize_input(clean, buf, sizeof(clean));

	char cmd[576];

	if (strcmp(path, "/ctl") == 0) {
		snprintf(cmd, sizeof(cmd), "%s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/current_workspace") == 0) {
		snprintf(cmd, sizeof(cmd), "workspace %s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/focus") == 0) {
		snprintf(cmd, sizeof(cmd), "focus %s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/mfact") == 0) {
		snprintf(cmd, sizeof(cmd), "mfact %s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/swallow") == 0 || strcmp(path, "/windows/active/swallow") == 0) {
		snprintf(cmd, sizeof(cmd), "toggle_swallow\n");
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/auto_swallow") == 0) {
		snprintf(cmd, sizeof(cmd), "auto_swallow %s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/windows/active/ctl") == 0) {
		snprintf(cmd, sizeof(cmd), "%s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	if (strcmp(path, "/windows/active/workspace") == 0) {
		snprintf(cmd, sizeof(cmd), "moveto %s\n", clean);
		ipc_query(cmd, last_ctl_reply, sizeof(last_ctl_reply));
		return (int)size;
	}

	return -EACCES;
}

static const struct fuse_operations mint_oper = {
	.getattr    = mint_getattr,
	.readdir    = mint_readdir,
	.open       = mint_open,
	.read       = mint_read,
	.write      = mint_write,
	.truncate   = mint_truncate,
};

static void print_usage(const char *prog) {
	printf("Usage: %s [FUSE options] <mountpoint>\n\n", prog);
	printf("MinTwm Plan 9-style virtual filesystem daemon.\n\n");
	printf("Virtual filesystem hierarchy:\n");
	printf("  ctl                      Execute any command / read last reply\n");
	printf("  current_workspace        Read active workspace / write to switch (1-9, next, prev, back)\n");
	printf("  title                    Read active window title\n");
	printf("  status                   Read JSON status\n");
	printf("  workspaces               Read workspace list with [active]\n");
	printf("  mfact                    Read or write master area factor (0.1 - 0.9)\n");
	printf("  focus                    Write next, prev, or master\n");
	printf("  swallow                  Read swallowing state (0 or 1) / write to toggle\n");
	printf("  auto_swallow             Read (0 or 1) / write (on/off/toggle) auto-swallowing\n");
	printf("  windows/active/title     Read active window title\n");
	printf("  windows/active/ctl       Write close, fullscreen, swap, or toggle_swallow\n");
	printf("  windows/active/workspace Write workspace number to move active window\n");
	printf("  windows/active/swallow   Read swallowing state (0 or 1) / write to toggle\n\n");
	printf("Examples:\n");
	printf("  mkdir -p ~/.mint && %s ~/.mint\n", prog);
	printf("  cat ~/.mint/current_workspace\n");
	printf("  echo 3 > ~/.mint/current_workspace\n");
	printf("  echo close > ~/.mint/windows/active/ctl\n");
	printf("  fusermount3 -u ~/.mint\n\n");
}

int main(int argc, char *argv[]) {
	if (argc > 1 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
		print_usage(argv[0]);
		return 0;
	}

  if (mkdir(argv[argc - 1], 0700) == -1 && errno != EEXIST) {
    perror("mkdir");
    return 1;
  }

	return fuse_main(argc, argv, &mint_oper, NULL);
}
