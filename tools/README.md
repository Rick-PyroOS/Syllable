# Editor setup: how clangd sees the Syllable tree

Syllable is built on the Syllable box itself, with plain Makefiles and GCC 4.1.2.
Editors on another machine (Neovim, VS Code, anything that uses clangd) need to
be told what that build would do: which headers, which macros, which target.
The tools in this folder work that out and write it to `compile_commands.json`,
which clangd reads.

## The short version

```sh
cp .syllable-box.example .syllable-box     # once; edit the host if yours differs
python3 tools/gen_compile_commands.py      # after changing Makefiles or adding folders
python3 tools/gen_compile_commands.py --sync   # after installing libraries on the box
```

Restart the editor (or clangd) after either one.

## Where headers come from

clangd looks for a header in this order:

| Order | Where | What |
|---|---|---|
| 1 | `system/sys/include` | Syllable's own headers: `<gui/...>`, `<util/...>`, `<atheos/...>`, `<posix/...>` |
| 2 | `system/libs/` | Libraries in the repo, by library name: `<codeview/codeview.h>` |
| 3 | `.sysroot/` | Everything the repo doesn't have, copied from the box: glibc 2.7, libstdc++ 4.1.2, freetype, OpenSSL, SDL |

Each file's own Makefile `-I` paths come before all three, as they would in a
real build. Two folders in `system/libs/` are deliberately left out: `glibc` and
`glibc-2.3.2` are old glibc sources, and the box runs glibc 2.7 (built from the
upstream tarball plus `system/apps/utils/Builder/packages/Syllable/glibc-2.7/`).

Why tier 3 has to come from the box: the C++ library ships with GCC, which isn't
built from this repo, and the installed glibc 2.7 headers are assembled by
glibc's own build. Once the GCC port is done, `.sysroot/` can be built from
source instead (the Docker cross-compiler will need exactly that).

## The pieces

| File | Committed | What it does |
|---|---|---|
| `tools/gen_compile_commands.py` | yes | Reads every Makefile, takes its `-I`/`-D`/`-U` flags, adds the base flags, and writes one entry per source file to `compile_commands.json`. Adds repo folders that `#include <dir/file.h>` lines need. Runs the sync first when `.sysroot/` is missing, or always with `--sync`. |
| `tools/sync_sysroot.py` | yes | Copies tier 3 from the box into `.sysroot/`. See below. |
| `tools/base_flags.txt` | yes | Flags every userland file gets: target, the box gcc's macros, the three header tiers. Comments explain each one. |
| `tools/base_flags_kernel.txt` | yes | The same for the kernel and drivers (`system/sys/kernel`, `misc/drivers`): adds `__KERNEL__`, `-ffreestanding`, no C++ library. |
| `.clangd` | yes | Language standard by file type (gnu99 for C, gnu++98 for C++ to match GCC 4.1.2; headers outside the kernel are C++). Skips indexing the old glibc sources. |
| `.syllable-box.example` | yes | Template for `.syllable-box`. |
| `.syllable-box` | no | Your box's ssh target, e.g. `root@172.16.226.58`. |
| `.sysroot/` | no | The copy of the box's headers, plus `gcc-macros.txt` (the box gcc's predefined macros). |
| `compile_commands.json` | no | Generated; contains this machine's absolute paths. |

There are no `compile_flags.txt` files on purpose. clangd uses whichever settings
file it finds first walking up from the file you open, so a `compile_flags.txt`
would override `compile_commands.json` for everything below it. With only
`compile_commands.json`, a file that isn't listed yet (a new file, a header)
borrows the flags of its nearest listed neighbour.

## How the sync works

`tools/sync_sysroot.py` (or `gen_compile_commands.py --sync`):

1. Reads the box's address from `.syllable-box` (or the `SYLLABLE_BOX` environment
   variable). With neither, it stops; tiers 1 and 2 still work.
2. Runs `gcc -v -E` on the box in a login shell and reads gcc's header search list.
   gcc's own built-in header folder is skipped, since clang brings its own.
3. Adds the box folders that Makefiles name with `-I` (e.g. `/usr/freetype/include`).
4. Copies each folder with rsync into `.sysroot/<same path as on the box>`.
   `--copy-unsafe-links` copies the real files behind Syllable's absolute symlinks.
   Nothing is written on the box.
5. Saves the box gcc's macros to `.sysroot/gcc-macros.txt`.
6. Rewrites the `-isystem .sysroot/...` lines in both base flags files to match
   gcc's search list.

It opens one shared ssh connection, so you're asked for the password once. The
box runs OpenSSH 5, so the script allows `ssh-rsa`, and keeps the box's host key
in `~/.ssh/known_hosts_syllable`: trusted the first time, checked after that.
Settings you can override in `.syllable-box`: `host`, `rsync_path` (default
`/usr/rsync/bin/rsync`), `ssh_options`, `ssh`.

To stop the password prompt, give the box an RSA key (OpenSSH 5 predates
ed25519): `ssh-keygen -t rsa`, then append the `.pub` file to the box's
`~/.ssh/authorized_keys`.

## Known gaps

- A few kernel files show errors at the top (`index_info` defined twice;
  `nlink_t`, `sigset_t`, `fd_set` clashes). Those are real conflicts between the
  kernel headers and glibc's, not an editor problem.
- `dbprintf()` is declared differently in `syllable/kdebug.h` and glibc's
  `sys/debug.h`. GCC 4.1 accepts it; newer compilers won't.
- When the GCC 15 port lands, change `gnu++98` to `gnu++20` in `.clangd`.
