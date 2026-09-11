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
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

static pthread_mutex_t ctl_mutex = PTHREAD_MUTEX_INITIALIZER;
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
static void sanitize_input(char *dst, const char *src, size_t src_len, size_t max) {
	if (max == 0) return;
	size_t limit = (src_len < max - 1) ? src_len : (max - 1);
	size_t i = 0;
	while (i < limit && src[i] != '\0') {
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

enum vfile_type {
	VFILE_REG,
	VFILE_DIR,
};

struct vfile_entry {
	const char *path;
	mode_t mode;
	enum vfile_type type;
	const char *parent_dir;
	const char *name;
	const char *read_cmd;
	const char *write_fmt;
};

static const struct vfile_entry vfiles[] = {
	/* Directories */
	{ "/",                    0755, VFILE_DIR, NULL,             NULL,        NULL,               NULL },
	{ "/windows",             0755, VFILE_DIR, "/",              "windows",   NULL,               NULL },
	{ "/windows/active",      0755, VFILE_DIR, "/windows",       "active",    NULL,               NULL },

	/* Root files */
	{ "/ctl",                 0666, VFILE_REG, "/",              "ctl",       NULL,               "%s\n" },
	{ "/current_workspace",   0666, VFILE_REG, "/",              "current_workspace", "get_workspace\n", "workspace %s\n" },
	{ "/title",               0444, VFILE_REG, "/",              "title",     "get_title\n",      NULL },
	{ "/status",              0444, VFILE_REG, "/",              "status",    "status\n",         NULL },
	{ "/workspaces",          0444, VFILE_REG, "/",              "workspaces", "get_workspaces\n", NULL },
	{ "/mfact",               0666, VFILE_REG, "/",              "mfact",     "mfact\n",          "mfact %s\n" },
	{ "/focus",               0222, VFILE_REG, "/",              "focus",     NULL,               "focus %s\n" },
	{ "/swallow",             0666, VFILE_REG, "/",              "swallow",   "get_swallow\n",    "toggle_swallow\n" },
	{ "/auto_swallow",        0666, VFILE_REG, "/",              "auto_swallow", "auto_swallow\n", "auto_swallow %s\n" },

	/* /windows/active files */
	{ "/windows/active/title",     0444, VFILE_REG, "/windows/active", "title",     "get_title\n",      NULL },
	{ "/windows/active/ctl",       0222, VFILE_REG, "/windows/active", "ctl",       NULL,               "%s\n" },
	{ "/windows/active/workspace", 0666, VFILE_REG, "/windows/active", "workspace", "get_workspace\n", "moveto %s\n" },
	{ "/windows/active/swallow",   0666, VFILE_REG, "/windows/active", "swallow",   "get_swallow\n",    "toggle_swallow\n" },
};

static const struct vfile_entry *find_entry(const char *path) {
	for (size_t i = 0; i < sizeof(vfiles) / sizeof(vfiles[0]); i++) {
		if (strcmp(vfiles[i].path, path) == 0) {
			return &vfiles[i];
		}
	}
	return NULL;
}

static int get_file_content(const struct vfile_entry *entry, char *buf, size_t max) {
	if (!entry || !buf || max == 0) return -ENOENT;

	if (strcmp(entry->path, "/ctl") == 0) {
		pthread_mutex_lock(&ctl_mutex);
		snprintf(buf, max, "%s", last_ctl_reply);
		pthread_mutex_unlock(&ctl_mutex);
		return 0;
	}

	if (strcmp(entry->path, "/mfact") == 0) {
		char reply[128] = {0};
		if (ipc_query("mfact\n", reply, sizeof(reply)) == 0) {
			if (strncmp(reply, "OK mfact ", 9) == 0) {
				snprintf(buf, max, "%s", reply + 9);
			} else {
				snprintf(buf, max, "%s", reply);
			}
			return 0;
		}
		return -EIO;
	}

	if (entry->read_cmd) {
		if (ipc_query(entry->read_cmd, buf, max) == 0) {
			return 0;
		}
		return -EIO;
	}

	return -ENOENT;
}

static int mint_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
	(void)fi;
	memset(stbuf, 0, sizeof(struct stat));

	const struct vfile_entry *entry = find_entry(path);
	if (!entry) {
		return -ENOENT;
	}

	if (entry->type == VFILE_DIR) {
		stbuf->st_mode = S_IFDIR | entry->mode;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | entry->mode;
		stbuf->st_nlink = 1;
		stbuf->st_size = 0; /* Direct I/O: size 0 avoids IPC query storm on stat/ls */
	}
	return 0;
}

static int mint_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
	(void)offset;
	(void)fi;
	(void)flags;

	const struct vfile_entry *dir = find_entry(path);
	if (!dir || dir->type != VFILE_DIR) {
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	for (size_t i = 0; i < sizeof(vfiles) / sizeof(vfiles[0]); i++) {
		if (vfiles[i].parent_dir && strcmp(vfiles[i].parent_dir, path) == 0) {
			filler(buf, vfiles[i].name, NULL, 0, 0);
		}
	}

	return 0;
}

static int mint_open(const char *path, struct fuse_file_info *fi) {
	const struct vfile_entry *entry = find_entry(path);
	if (!entry || entry->type != VFILE_REG) {
		return -ENOENT;
	}
	/* Direct I/O disables page cache so file reads always reflect live state */
	fi->direct_io = 1;
	return 0;
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
	const struct vfile_entry *entry = find_entry(path);
	if (!entry || entry->type != VFILE_REG) {
		return -ENOENT;
	}
	if (!(entry->mode & 0444)) {
		return -EACCES;
	}

	char content[2048] = {0};
	int res = get_file_content(entry, content, sizeof(content));
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

	const struct vfile_entry *entry = find_entry(path);
	if (!entry || entry->type != VFILE_REG) {
		return -ENOENT;
	}
	if (!(entry->mode & 0222) || !entry->write_fmt) {
		return -EACCES;
	}

	char clean[512];
	sanitize_input(clean, buf, size, sizeof(clean));

	char cmd[576];
	if (strchr(entry->write_fmt, '%')) {
		snprintf(cmd, sizeof(cmd), entry->write_fmt, clean);
	} else {
		snprintf(cmd, sizeof(cmd), "%s", entry->write_fmt);
	}

	char reply[1024] = {0};
	ipc_query(cmd, reply, sizeof(reply));

	pthread_mutex_lock(&ctl_mutex);
	memcpy(last_ctl_reply, reply, sizeof(last_ctl_reply));
	pthread_mutex_unlock(&ctl_mutex);

	return (int)size;
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
	if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
		print_usage(argv[0]);
		return (argc < 2) ? 1 : 0;
	}

	const char *mountpoint = argv[argc - 1];
	if (mountpoint[0] != '-') {
		if (mkdir(mountpoint, 0700) == -1 && errno != EEXIST) {
			perror("mkdir");
			return 1;
		}
	}

	return fuse_main(argc, argv, &mint_oper, NULL);
}
