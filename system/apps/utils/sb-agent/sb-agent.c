/*
 * sb-agent: the Syllable end of Syllable-Build-System's ./sb execute.
 *
 * Listens on a TCP port (default 2346; ./sb run forwards the Mac's
 * localhost:2346 here) and takes one request per connection, a line of
 * tab-separated fields:
 *
 *   PING                         -> "sb-agent 1"
 *   LIST                         -> the programs on the boot volume, one per line
 *   PUT  mode size path          followed by size bytes: writes path (making its
 *                                folders) with the octal mode -> "OK" or "ERR ..."
 *   RUN  program [args...]       runs it; its output (stdout and stderr) comes
 *                                back on the connection, then
 *                                "[sb-agent] exit N".  Closing the connection
 *                                stops the program.
 *
 * Only connections from the QEMU host (10.0.2.2) or this machine are taken:
 * it runs programs as root, so it starts on ./sb image's images only, not on a
 * release's.
 *
 *   sb-agent [port]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define LINE_MAX_LEN 8192
#define MAX_ARGS 64

static int write_all(int fd, const char *buf, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, buf, len);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		buf += n;
		len -= n;
	}
	return 0;
}

static int write_str(int fd, const char *s)
{
	return write_all(fd, s, strlen(s));
}

/* One request line, without its newline; a byte at a time, so a PUT's data
   stays in the socket */
static int read_line(int fd, char *buf, size_t size)
{
	size_t len = 0;
	for (;;) {
		char c;
		ssize_t n = read(fd, &c, 1);
		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		if (c == '\n')
			break;
		if (len + 1 >= size)
			return -1;
		buf[len++] = c;
	}
	if (len > 0 && buf[len - 1] == '\r')
		len--;
	buf[len] = 0;
	return 0;
}

static int split_tabs(char *line, char **fields, int max)
{
	int n = 0;
	char *p = line;
	while (n < max - 1) {
		fields[n++] = p;
		p = strchr(p, '\t');
		if (p == NULL)
			break;
		*p++ = 0;
	}
	fields[n] = NULL;
	return n;
}

static int is_elf(const char *path)
{
	unsigned char magic[4];
	int fd = open(path, O_RDONLY);
	int ok = 0;
	if (fd < 0)
		return 0;
	if (read(fd, magic, 4) == 4 && magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F')
		ok = 1;
	close(fd);
	return ok;
}

/* ELF executables under dir; folders are entered only when they are real
   folders (no link loops), files are followed (the index links) */
static void list_dir(int out, const char *dir, int depth)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	if (d == NULL)
		return;
	while ((e = readdir(d)) != NULL) {
		char path[1024];
		struct stat st;
		if (e->d_name[0] == '.')
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
		if (lstat(path, &st) < 0)
			continue;
		if (S_ISDIR(st.st_mode)) {
			if (depth > 0)
				list_dir(out, path, depth - 1);
			continue;
		}
		if (stat(path, &st) < 0 || !S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR))
			continue;
		if (is_elf(path)) {
			write_str(out, path);
			write_str(out, "\n");
		}
	}
	closedir(d);
}

static void do_list(int sock)
{
	static const char *dirs[] = {
		"/boot/applications", "/boot/system/programs", "/boot/system/development/bin",
		"/boot/usr/index/bin", "/boot/usr/index/sbin", "/boot/bin", "/boot/usr/sb", NULL
	};
	int i;
	for (i = 0; dirs[i] != NULL; i++)
		list_dir(sock, dirs[i], 4);
}

static int make_parents(char *path)
{
	char *p;
	for (p = path + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = 0;
		if (mkdir(path, 0755) < 0 && errno != EEXIST) {
			*p = '/';
			return -1;
		}
		*p = '/';
	}
	return 0;
}

static void do_put(int sock, char **f, int n)
{
	char buf[16384], tmp[1100], msg[1200];
	long size, left;
	int mode, fd;
	if (n != 4 || f[3][0] != '/') {
		write_str(sock, "ERR usage: PUT mode size /path\n");
		return;
	}
	mode = (int)strtol(f[1], NULL, 8);
	size = strtol(f[2], NULL, 10);
	if (make_parents(f[3]) < 0) {
		snprintf(msg, sizeof(msg), "ERR mkdir for %s: %s\n", f[3], strerror(errno));
		write_str(sock, msg);
		return;
	}
	/* written next to it and renamed: a running program keeps its old file */
	snprintf(tmp, sizeof(tmp), "%s.sb-agent", f[3]);
	fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fd < 0) {
		snprintf(msg, sizeof(msg), "ERR %s: %s\n", tmp, strerror(errno));
		write_str(sock, msg);
		return;
	}
	for (left = size; left > 0;) {
		ssize_t r = read(sock, buf, left < (long)sizeof(buf) ? left : (long)sizeof(buf));
		if (r < 0 && errno == EINTR)
			continue;
		if (r <= 0 || write_all(fd, buf, r) < 0) {
			close(fd);
			unlink(tmp);
			write_str(sock, "ERR short write\n");
			return;
		}
		left -= r;
	}
	close(fd);
	chmod(tmp, mode);
	if (rename(tmp, f[3]) < 0) {
		snprintf(msg, sizeof(msg), "ERR rename %s: %s\n", f[3], strerror(errno));
		unlink(tmp);
		write_str(sock, msg);
		return;
	}
	write_str(sock, "OK\n");
}

static void do_run(int sock, char **f, int n)
{
	char msg[256];
	pid_t pid;
	int status = 0, gone = 0;
	if (n < 2) {
		write_str(sock, "ERR usage: RUN program [args...]\n");
		return;
	}
	pid = fork();
	if (pid < 0) {
		write_str(sock, "ERR fork failed\n");
		return;
	}
	if (pid == 0) {
		int null = open("/dev/null", O_RDONLY);
		setsid();
		if (null >= 0)
			dup2(null, 0);
		dup2(sock, 1);
		dup2(sock, 2);
		if (sock > 2)
			close(sock);
		signal(SIGPIPE, SIG_DFL);
		execvp(f[1], f + 1);
		fprintf(stderr, "[sb-agent] %s: %s\n", f[1], strerror(errno));
		_exit(127);
	}
	/* Wait for it, and stop it when the Mac closes the connection (sb execute
	   interrupted).  Nothing more is sent to us, so readable means closed. */
	for (;;) {
		fd_set rd;
		struct timeval tv;
		pid_t w = waitpid(pid, &status, WNOHANG);
		if (w == pid)
			break;
		FD_ZERO(&rd);
		FD_SET(sock, &rd);
		tv.tv_sec = 0;
		tv.tv_usec = 250000;
		if (!gone && select(sock + 1, &rd, NULL, NULL, &tv) > 0) {
			char c;
			if (recv(sock, &c, 1, 0) <= 0) {
				gone = 1;
				kill(-pid, SIGTERM);
				kill(pid, SIGTERM);
			}
		} else if (gone) {
			usleep(250000);
		}
	}
	if (gone)
		return;
	if (WIFEXITED(status))
		snprintf(msg, sizeof(msg), "\n[sb-agent] exit %d\n", WEXITSTATUS(status));
	else
		snprintf(msg, sizeof(msg), "\n[sb-agent] killed by signal %d\n", WTERMSIG(status));
	write_str(sock, msg);
}

static void serve(int sock)
{
	char line[LINE_MAX_LEN];
	char *f[MAX_ARGS + 1];
	int n;
	if (read_line(sock, line, sizeof(line)) < 0)
		return;
	n = split_tabs(line, f, MAX_ARGS + 1);
	if (strcmp(f[0], "PING") == 0)
		write_str(sock, "sb-agent 1\n");
	else if (strcmp(f[0], "LIST") == 0)
		do_list(sock);
	else if (strcmp(f[0], "PUT") == 0)
		do_put(sock, f, n);
	else if (strcmp(f[0], "RUN") == 0)
		do_run(sock, f, n);
	else
		write_str(sock, "ERR unknown request\n");
}

int main(int argc, char **argv)
{
	int port = argc > 1 ? atoi(argv[1]) : 2346;
	int lsock, one = 1;
	struct sockaddr_in addr;

	signal(SIGPIPE, SIG_IGN);
	lsock = socket(AF_INET, SOCK_STREAM, 0);
	if (lsock < 0) {
		perror("sb-agent: socket");
		return 1;
	}
	setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(lsock, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(lsock, 8) < 0) {
		perror("sb-agent: bind");
		return 1;
	}
	printf("sb-agent: listening on port %d\n", port);
	fflush(stdout);

	for (;;) {
		struct sockaddr_in peer;
		socklen_t plen = sizeof(peer);
		unsigned long ip;
		int sock = accept(lsock, (struct sockaddr *)&peer, &plen);
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
		if (sock < 0)
			continue;
		ip = ntohl(peer.sin_addr.s_addr);
		if (ip != 0x0a000202UL && ip != 0x7f000001UL) {     /* 10.0.2.2, 127.0.0.1 */
			printf("sb-agent: refused %s\n", inet_ntoa(peer.sin_addr));
			fflush(stdout);
			close(sock);
			continue;
		}
		if (fork() == 0) {
			close(lsock);
			serve(sock);
			close(sock);
			_exit(0);
		}
		close(sock);
	}
}
