# Pyro default image: Unix utilities audit

Nov 9, 2026 · @Rick

## Summary

Three of Rick's five are already on the image: **cat** (coreutils 6.4), **less** (429) and **nano** (2.0.3). **more** exists only as a bash alias for less, so scripts and non-interactive shells can't run it. **jq** isn't there and has no recipe anywhere in the tree.

The image ships about 390 distinct command names, so the base is broader than it feels. The biggest gaps are:

- **No `man`**, even though about 1,170 man pages are installed. `MANPATH` also points at index folders that are empty.
- **No `vi`/`vim`**, which every Unix user and many scripts expect.
- **coreutils 6.4 (2006) is too old** for `timeout`, `realpath`, `nproc`, `truncate` and `stdbuf`, which modern build scripts call all the time.
- **No `jq`, `bc`, `hexdump`/`xxd`, `pgrep`/`pkill`, `watch`, `tree`, `nc`, `sudo`, `lua`, `sqlite3`, `python`.**

The quickest wins are a `more` wrapper, jq, tree, bc, Lua, the sqlite3 shell and a `man` (mandoc). coreutils 8.x and vim are bigger but matter most.

## Method and sources

Read-only. Nothing was built, run or changed.

- **What ships:** `Syllable-Build-System/out/syllable.manifest` (Nov 8 build, 17,182 entries). I listed every file and link in a `bin`/`sbin` folder, plus `/system/programs`.
- **What's on PATH:** `Syllable/system/image/etc/profile`. `/resources` links to `/usr`, so `/resources/index/bin` is `/usr/index/bin`.
- **What's already ported or has a recipe:** `Syllable/cmake/Ports.cmake` and `cmake/ports/*.cmake`, `Syllable-Ports/`, and the 851 old recipes in `Syllable/system/apps/utils/Builder/packages/`.
- **Earlier work:** `docs/image-contents.md` and `docs/posix-audit-2026-10-30.md` in the project files.

Not checked: whether each shipped tool actually runs on the VM. That needs a boot.

## What the image ships today

| Group | Commands | Package (version) |
| --- | --- | --- |
| File and text basics | cat, head, tail, ls, cp, mv, rm, ln, mkdir, chmod, chown, stat, touch, du, dd, sort, uniq, cut, tr, wc, tee, paste, join, split, csplit, od, nl, fmt, fold, pr, tac, shuf, seq, expr, printf, env, date, sleep, base64, md5sum, sha1sum to sha512sum, install, readlink, mknod, mkfifo, kill, uptime, who, whoami, su, uname, hostname | coreutils (6.4) |
| Shell | bash, sh | bash (4.0) |
| Paging and editing | less, lessecho, lesskey, nano, rnano, ed, red | less (429), nano (2.0.3), ed (0.9) |
| Search | grep, egrep, fgrep, find, xargs, locate, updatedb, which, file | grep (2.5.4), findutils (4.4.2), which (2.19), file (5.11) |
| Text processing | sed, awk/gawk, diff, diff3, cmp, sdiff, patch, iconv | sed (4.2.1), gawk (3.1.6), diffutils (2.8.1), patch (2.5.4), glibc |
| Archives | tar, gzip, zcat, bzip2, xz, lzma, zip, unzip | tar (1.22), gzip (1.3.12), bzip2 (1.0.5), xz (4.999.9beta), zip (3.0), unzip (6.0) |
| Network | curl, wcurl, ssh, scp, sftp, ssh-keygen, sshd, rsync, telnet, ftp, inetd, openssl | curl (8.23.0), OpenSSH (10.6p1), rsync (3.5.1), inetutils (1.6), OpenSSL (3.5.9) |
| Terminal | clear, reset, tput, tset, tic, infocmp | ncurses (5.7) |
| Syllable's own (`/system/programs`) | ps, top, kill\_all, strace, sysinfo, df, mount, unmount, format, ifconfig, route, ping, dhcpc, shutdown, reboot, open, cbcopy, cbpaste, ls\_ports, ls\_sema | Syllable tree |
| Development | gcc and g++ (4.1.2 and 9.5.0), make, m4, bison, flex, gperf, autoconf, automake, libtool, pkg-config, git, perl, doxygen, makeinfo, plus binutils' strings, nm, objdump, readelf | ports |

The binutils tools (strings, objdump and so on) live in `/usr/gcc/<version>/bin`, so they're only on PATH after `use-gcc`.

## What's missing

Rick's five come first, then the rest by group. "Recipe" means an old Builder recipe exists in `Builder/packages`, which helps with patches but is usually years out of date.

| Tool | Status | Where a port could start | Notes |
| --- | --- | --- | --- |
| cat | Ships | coreutils 6.4 | Nothing to do. |
| less | Ships | less 429 | Old (2008). Fine for now. |
| more | Alias only | Wrapper script, or a newer less | `alias more=less` in `/etc/profile` works only in interactive bash. |
| nano | Ships | nano 2.0.3 | Old (2008), no syntax colouring. Not the default `EDITOR` (that's aedit). |
| jq | Missing | No recipe | jq 1.7 is plain C with a bundled decNumber. Build with gcc 9.5 and `--without-oniguruma`. |
| man | Missing | Builder `Man` (a small script), or mandoc | 1,169 man pages ship. `/usr/index/manuals` and `/system/index/manuals` are empty. |
| vi / vim | Missing | Recipes vim-6.2 to 7.0 | vim also brings `xxd`. |
| timeout, realpath, nproc, truncate, stdbuf | Missing | Recipes CoreUtilities-6.7 and 8.4 | All need coreutils 7 or 8. gnulib should cover the missing `*at()` calls, but that needs testing. |
| bc, dc | Missing | No recipe | GNU bc 1.07 is plain C and can use the readline that ships. |
| hexdump, xxd | Missing | vim (xxd) | `od -A x -t x1z` works as a stopgap. |
| pgrep, pkill, killall | Missing | None suitable | Syllable has `ps` and `kill_all`. procps and psmisc need Linux `/proc`. A small native `pgrep`/`pkill` over the Syllable process API is the realistic path. |
| watch | Missing | None | About 100 lines of C over ncurses, or a shell function. |
| time (`/usr/bin/time`) | Missing | GNU time 1.9 | Bash's `time` keyword covers most use. |
| tree | Missing | Recipes unixtree, ytree (different tools) | tree 2.x is one small C program. |
| column, rev | Missing | None | Both are small util-linux tools. Portable copies exist in the BSDs. |
| sudo | Missing | Recipes sudo-1.6.9p9, SUDo-1.7.2p2 | `/etc/profile` defines `alias s=sudo`, which fails today. Weak value until the VFS checks permissions (posix audit item 5). |
| passwd, useradd | Missing | None | Only the GUI Users app source exists, and it isn't built. |
| nc (netcat) | Missing | Recipe NetCat-1.10 | Handy for testing the network stack. |
| wget | Missing | Recipes wget-1.9.1, 1.10.2 | `wcurl` (ships with curl 8) covers simple downloads. |
| host, dig, nslookup | Missing | None | `getent hosts name` works today through glibc. |
| netstat, traceroute, lsof | Missing | None | Need kernel interfaces Syllable doesn't have. Better as native tools later. |
| zstd, lz4, cpio | Missing | Syllable-Ports/zstd-1.0.0 (old) | zstd 1.5 builds from a plain Makefile. |
| lua | Missing | Recipes Lua-5.1.5, lua-5.1.1 | The release shipped Lua. Lua 5.4 is ANSI C. |
| sqlite3 (the shell) | Missing | sqlite 3.5.5 is already built, as a static library only | Shipping the shell is close to free. A newer sqlite would be better. |
| python3 | Missing | Recipes Python 2.5 to 3.1 | Large. Hits the thread, semaphore and AF\_UNIX gaps in the posix audit. |
| cmake | Missing | Recipes cmake 2.4 and 2.8 | The release had it. Needs a C++ compiler (gcc 9.5 is there). |
| tmux, screen | Missing | None | tmux needs AF\_UNIX sockets (posix audit item 2). screen can run over FIFOs. |
| gdb | Missing | gdbserver 18.1 ships | Remote debugging from the Mac already works, so a native gdb is low priority. |
| dos2unix, script, cal | Missing | Recipe dos2unix-3.1 | Small and low priority. |

## Config problems found along the way

These are in `Syllable/system/image/etc/profile`, and each is a one-line fix.

- **`EDITOR=aedit`** is a GUI editor. Over SSH, `git commit` and anything else that opens `$EDITOR` won't give you a usable editor in the terminal. Suggest `EDITOR=nano` (and `VISUAL=nano`) for shells without a desktop, or always.
- **`alias s=sudo`** points at a program that doesn't exist.
- **`TERMINFO=/usr/share/terminfo`** doesn't exist on the image. The database is in `/system/resources/ncurses/5.7/share/terminfo`. Programs still work because ncurses falls back to its built-in path, but the variable is wrong. Point it at the real folder, or add a link.
- **`MANPATH`** lists `/resources/index/manuals` and `/system/index/manuals`, and both are empty. The ports install man pages under `/usr/<package>/<version>/share/man`, and nothing links them into the index.
- **`LESSCHARSET=latin1`** shows UTF-8 text (git logs, newer man pages) as escape codes. Worth checking whether aterm can show UTF-8 before changing it.

## Prioritized add list

Ordered by value for the effort. Each item would be its own branch and pull request, following the build system's one-change-per-branch rule.

**Tier 1: quick wins (a day or less each)**

1. **`more`:** a two-line `/usr/index/bin/more` script (`exec less -E "$@"`). Drop the alias afterwards.
2. **Profile fixes:** `EDITOR`/`VISUAL=nano`, the right `TERMINFO`, and remove `alias s=sudo`.
3. **jq 1.7.1:** plain C with gcc 9.5 and `--without-oniguruma`. It's the one on Rick's list that's truly missing.
4. **tree 2.x**, **bc 1.07** and **GNU time 1.9:** small, self-contained C.
5. **sqlite3 shell:** the library is already built; install `sqlite3` as well. A later step can move to a newer sqlite.
6. **Lua 5.4:** ANSI C, no configure, and useful for scripts.

**Tier 2: high value, moderate effort**

7. **`man`:** port mandoc (one C program, reads the roff pages directly, no groff). Then link each port's `share/man` into `/usr/index/manuals` so `MANPATH` finds them.
8. **coreutils 8.x:** brings `timeout`, `realpath`, `nproc`, `truncate`, `stdbuf`, `base32` and many option fixes that modern build scripts expect. The 8.4 recipe is a starting point. Most of the risk is in gnulib's fallbacks for the missing `*at()` calls.
9. **vim 9:** gives `vi`, `vim` and `xxd`. It builds with gcc 9.5 against ncurses 5.7.
10. **netcat, zstd, dos2unix, watch, column, rev:** each is small. `column` and `rev` can come from a BSD.

**Tier 3: larger projects, or blocked on the kernel**

11. **pgrep and pkill:** write native ones over Syllable's process API, next to `ps` and `kill_all`.
12. **passwd and useradd:** native command-line tools, or build the Users app.
13. **cmake and python3:** large. Python especially hits the gaps in the posix audit (threads, semaphores, AF\_UNIX).
14. **sudo:** wait until the VFS enforces permissions, otherwise it adds little.
15. **tmux:** blocked on AF\_UNIX sockets. screen is the fallback.

## Out of reach for now

The newer tools many people now count as standard, such as ripgrep, fd, bat, eza, fzf, htop and btop, need a Rust or Go compiler that targets Syllable, or Linux's `/proc`. Neither exists, so they aren't on this list. For a C alternative, `the_silver_searcher` (ag) is closest to ripgrep. BusyBox could fill many small gaps at once, but it assumes Linux throughout, so porting the separate tools is less work.
