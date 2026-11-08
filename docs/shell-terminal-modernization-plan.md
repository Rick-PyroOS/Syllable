# Shell, environment and Terminal: audit and modernization plan

2026-11-08. Read from `~/Code/Rick-PyroOS/Syllable` (branch `feat/docs`, e4d8ade4) and the current build manifest `Syllable-Build-System/out/syllable.manifest`. Nothing was run on the Syllable box; items marked **verify** need a quick check there (commands in Appendix A).

## 1. Summary

The shell is fine as a base, but everything around it is from 2006–2011, and the Terminal is a 1999 AtheOS VT100 subset. The biggest problems:

1. **Colour is turned off for every curses program.** `/etc/profile:101` forces `TERM=xterm-old`, which in ncurses 5.7 is a link to `xterm-r6`, an entry with no colour capabilities. It also overrides the `TERM=xterm` the Terminal sets, and the `TERM` that sshd passes in from your Mac.
2. **No UTF-8 anywhere.** No locales are compiled (`/system/resources/glibc/2.7/lib/locale` isn't in the image), `LANG` is unset, `LESSCHARSET=latin1`, and the pty line discipline erases bytes, not characters. The Terminal decodes UTF-8 but has no character widths, and corrupts a character split across two `read()` calls.
3. **The Terminal's parser is ad hoc.** It buffers each escape sequence as a string and re-parses it with `atoi`. It handles about 30 sequences, has no 256 or truecolor, no alternate screen, no cursor show/hide, no bracketed paste, no mouse, no device reports, and only OSC 0 for titles. Several bugs can crash it or corrupt memory (Section 4.6).
4. **Old userland.** bash 4.0 (2009), readline 6.0, ncurses 5.7 (narrow, 2008), coreutils 6.4 (2006), less 429, nano 2.0.3, grep 2.5.4. The newer ports (OpenSSH 10.6p1, OpenSSL 3.5.9, git 2.47, perl 5.40) show the toolchain can now build current versions.
5. **Shell startup has rough edges:** `./` on root's PATH, a prompt with literal brackets, no `~/.bashrc` or `/etc/inputrc`, and a GUI editor as `EDITOR`.

"Modern standard" in this plan means: a UTF-8 locale by default, `TERM=xterm-256color` and `COLORTERM=truecolor` that are honest, and a Terminal that passes the core of `vttest` and esctest and runs Neovim, tmux, less, htop-style TUIs and git's pager correctly. That is roughly what foot, Alacritty and Ghostty implement.

The plan has nine stages (Section 5). Stages 0 to 3 can start now and don't touch the parser. Stage 5 is your rewrite of the line parsing after Cairo, and Section 6 specifies what it should cover.

## 2. What's on the image today

| Piece | Version / location | Notes |
|---|---|---|
| Shell | bash 4.0, `/system/resources/bash/4.0`, linked as `/bin/bash` and `/bin/sh` | `cmake/Ports.cmake:4`. Built with its bundled readline (no `--with-installed-readline`, unlike the old Builder recipe). |
| Line editing | readline 6.0 (static), plus bash's own bundled copy | `--enable-multibyte` |
| Terminal database | ncurses 5.7, static, narrow (no `--enable-widec`), terminfo in `/system/resources/ncurses/5.7/share/terminfo` | `xterm-256color` exists; no `tmux-256color`, no `xterm-direct`. |
| termcap | `/etc/termcap` (10,445 lines, old GNU file) | Its `xterm` entry is a hand-tuned 1990s one. It has no `xterm-old`. |
| Locale | glibc 2.7, `locale` and `localedef` and the `share/i18n` sources ship; no compiled locales | Matches posix audit P2-4. |
| Pager / editors | less 429, nano 2.0.3, AEdit (GUI) | No vi on the base image. |
| Core tools | coreutils 6.4, grep 2.5.4, sed 4.2.1, gawk 3.1.6, findutils 4.4.2, tar 1.22 | |
| Terminal | `system/apps/utils/aterm` (4,393 lines), installed as `/applications/system/Terminal` and `/system/programs/aterm` | Version string "1.0.0c". |
| PTY | BSD-style `/dev/pty/mst/ptyN` and `/dev/pty/slv/ptyN`, kernel `vfs/pty.c` | `posix_openpt` is a stub (posix audit P0-3). |
| Startup files | `/etc/profile`, `/system/user-setup.sh`, `~/.profile` (root and `.template`) | No `/etc/bash.bashrc`, `~/.bashrc`, `/etc/inputrc`, `/etc/profile.d/`. |

## 3. Shell and environment findings

All `/etc/profile` line numbers refer to `system/image/etc/profile`.

### 3.1 TERM, TERMINFO, COLORTERM

- **`TERM=xterm-old` (line 101).** The 2011 ChangeLog says it was chosen because it "works somewhat less badly". In ncurses 5.7 it's `xterm-r6`: 8 function keys, no `setaf`/`setab`, so nano, less, mc and anything using curses get monochrome. `ls --color` still works because it ignores terminfo.
- **It overrides the right value.** The Terminal already does `setenv("TERM", "xterm")` before exec (`aterm/main.cpp:519`), but it runs `bash --login`, which sources `/etc/profile` and replaces it. Over SSH, sshd passes the client's `TERM` (Ghostty sends `xterm-ghostty`) and `/etc/profile` clobbers that too. **`TERM` should be set only by whatever owns the terminal**, never by a profile.
- **`TERMINFO=/usr/share/terminfo` (line 100) points nowhere.** The image has no `/usr/share/terminfo`; the database is under `/system/resources/ncurses/5.7/share/terminfo`. ncurses 5.7 falls back to its compiled-in path after `$TERMINFO`, so this is probably harmless, but it's wrong and confusing (**verify** with `infocmp`).
- **`COLORTERM=xterm-old` (line 104)** isn't a meaningful value. By convention `COLORTERM` is `truecolor` or `24bit` when the terminal does RGB colour, and unset otherwise.
- **bash's own line editor may not use terminfo at all.** bash 4.0 picks the first of `libtermcap`, `libtinfo`, `libcurses`, `libncurses`, otherwise its bundled termcap reader, which uses `/etc/termcap`. In `Ports.cmake` bash is built before ncurses and doesn't depend on it, so it may be using `/etc/termcap`, where `xterm-old` doesn't exist and readline falls back to a dumb terminal (**verify**).

### 3.2 Locale and character set

- No locale is compiled, so only `C`/`POSIX` works; `/system/user-setup.sh` only has commented-out examples of `LANG`.
- With `LANG` unset, readline treats UTF-8 bytes as 8-bit meta characters (typing `é` shows `\303\251`, or inserts garbage), `wc -m` counts bytes, and `sort`/`grep` use byte order.
- **`LESSCHARSET=latin1` (line 116)** makes less show UTF-8 as two Latin-1 characters each. Remove it; less picks the charset from the locale.
- The pty line discipline (`vfs/pty.c:926`) erases one byte per backspace in canonical mode and has no `IUTF8` flag, so backspacing over `é` at a `cat` or `read` prompt leaves half a character.

### 3.3 PATH and search paths

- **`~/.profile` appends `./` to PATH** (`home/root/.profile`, `home/.template/.profile`). As root, by default, that means running whatever is in the current directory if a name doesn't resolve earlier. Remove it.
- The system PATH (line 18) is long and mixes three layouts (`/resources/index/*`, `/system/index/*`, `/usr/local/*`). That's the Syllable layout and can stay, but the empty and duplicate entries should be checked once `/resources` vs `/usr` is settled (the ports install to `/usr/...` and link from `/usr/index/bin`).
- `use_gcc 4.1.2` runs at every login and prepends `/usr/gcc/4.1.2/bin`. Fine for now; once GCC 9.5/15 is the default this becomes the place to switch.
- `C_INCLUDE_PATH`, `LIBRARY_PATH`, `PKG_CONFIG_PATH` are exported to every interactive shell. That's convenient on a self-hosting box, but they leak into any build and override what configure scripts detect. Consider moving them behind `use-gcc` so a plain shell has none set.

### 3.4 Startup file structure

- **Only login shells are configured.** Aliases, the title-setting `PROMPT_COMMAND` and `use-gcc` live in `/etc/profile`, so a plain `bash`, `sudo -s`, `su` without `-`, or a shell inside tmux gets none of them. There is no `/etc/bash.bashrc` or `~/.bashrc`, and `.profile` doesn't source one.
- **Every Terminal window is a login shell** (`bash --login`, `main.cpp:371`), so `/etc/profile` and all `tasks/setup/*` scripts rerun per window. macOS does the same, so that's acceptable, but the setup-task loop then belongs in `/etc/profile` and per-shell settings in `bashrc`.
- **The setup loops are fragile** (lines 128–136): `for setup in \`ls dir\`` breaks on spaces and prints an error if the directory doesn't exist. Use `for f in dir/*; do [ -r "$f" ] && . "$f"; done`.
- `export PWD` (line 7) does nothing useful; `~/.profile` ends with `cd $HOME`, which a login shell already does.
- The Builder's copy (`apps/utils/Builder/resources/configuration/profile`) has drifted from the image's. Delete it or regenerate it from the image's copy so there is one source.

### 3.5 Interactive defaults

- **Prompt:** `PS1=['\u@\h:\w']` prints literal brackets and no `$`/`#` or trailing space. Modern default: `PS1='\u@\h:\w\$ '`, and if colour is added, wrap escapes in `\[ \]`.
- **Title:** `PROMPT_COMMAND` (line 110) works, but it only runs in login shells; it should move to bashrc and use `printf '\033]0;%s\007'`.
- **No `/etc/inputrc`.** Without it readline doesn't know Home/End/Delete/Ctrl-arrows for this Terminal, and doesn't get `set enable-bracketed-paste on`, `set colored-stats on`, etc.
- **History:** bash defaults (500 lines, no `histappend`, no `HISTCONTROL`). Suggest `HISTSIZE=10000`, `HISTCONTROL=ignoreboth`, `shopt -s histappend checkwinsize`. `checkwinsize` is off by default in bash 4.0, so line wrapping goes wrong after resizing until the next command.
- **`EDITOR=aedit`** is a GUI app, so `git commit` or `crontab -e` over SSH fails. Use `EDITOR=nano` (or nvim once it's ported) and `VISUAL` for the GUI editor only when a desktop session is present.
- **Missing standard variables:** `LANG`, `TZ` (commented out), `XDG_CONFIG_HOME`/`XDG_DATA_HOME`/`XDG_CACHE_HOME`/`XDG_RUNTIME_DIR` (Neovim, git and most current tools use them), `SHELL` (check that login sets it).
- `alias s=sudo` plus root-by-default is fine for now but goes with the security-hardening stage on the roadmap.

## 4. Terminal findings

### 4.1 Structure

- One window per process. Globals hold the window, view, scrollbar and master pty (`main.cpp:77-96`).
- Three threads: the read thread (`ReadPTY`) does a blocking `read()` of 4 KB, locks the window and calls `TermView::Write()`; a refresh thread redraws every 40 ms by polling (`main.cpp:330`); the window thread handles input. Parsing and the screen model run under the window lock, so heavy output blocks input.
- `TermView` (`tview.cpp`, 2,914 lines) is the parser, the screen model, the scrollback, selection, rendering and keyboard encoding in one class.

### 4.2 Screen model

- Cells are `int32` code point plus `int16` attributes, 4-bit foreground and background (`tview.h:40-52`). No room for 256 or RGB colour, italic, strikethrough or underline styles.
- **Every line is a fixed 200 cells** (`tview.cpp:140`) but the width comes from the window, and `PutChar`, `EraseToEndOfLine`, `InsertChars` etc. index up to the window width. A window wider than 200 columns (fullscreen at 1920 px with an 8 px font is 240) writes past the heap arrays.
- Scrollback is capped at about 1,500 lines (`tview.cpp:1421`) and moves with `memmove` of the whole line-pointer array on every scrolled line.
- No alternate screen buffer. `ESC[?47h` sets `m_nCurrentBuffer = 1`, which only stops lines going to scrollback. Leaving vim or less doesn't restore what was there; `1047`/`1049` are rejected.
- No soft-wrap flag on lines, so copying a wrapped line inserts newlines and nothing can reflow on resize.
- No character widths: CJK and emoji take one cell, combining marks take a cell of their own.

### 4.3 Parser (`AddEscCode`, `ParseBraceEsc`, `ParseParanEsc`, `Write`)

How it works: `Write()` walks the buffer one UTF-8 character at a time. On ESC it starts appending bytes to `m_zEscString[512]`; when a CSI final byte arrives it re-parses the string, collects numbers with `atoi`, and dispatches on the final byte.

Problems:

- **Not a state machine.** C0 controls inside a sequence (legal, and common: `ESC[1;\r5H`) are appended to the string, not executed. CAN/SUB don't abort. Intermediate bytes (`ESC[ q`, `ESC[!p`) aren't recognised, so the final byte is misread.
- **Parameters:** empty parameters are dropped instead of defaulted (`ESC[;5H` becomes `ESC[5H`), there is no bound on the 16-entry `argv` (`tview.cpp:2016`, an overflow with 17+ parameters, e.g. long SGR strings), and colon subparameters (`38:2::r:g:b`) aren't parsed.
- **UTF-8 is decoded before the parser** using `utf8_char_length()` of the first byte, then `i += len`. A character split across two reads runs off the end of the 4 KB buffer (`tview.cpp:2520`). Inside an escape string only the first byte of each character is kept, so UTF-8 window titles are cut to lead bytes. Malformed input isn't replaced with U+FFFD.
- **OSC:** only `ESC]0;…BEL`. OSC 1/2/7/8/52/4/10/11/133 are rejected, and the ST terminator (`ESC \`) isn't recognised, so an ST-terminated title swallows following output until a BEL arrives. The title buffer is a function-local `static` that isn't reset on an invalid sequence, so the next title starts with leftovers.
- **Wrong implementations:** `CSI G` (cursor to column) does a carriage return (`tview.cpp:2109`); `` CSI ` `` (HPA) is off by one (`:2185`); `CSI J` with `0` erases correctly but `CSI 3 J` (clear scrollback) is ignored; `ESC 8` doesn't redraw the restored cursor line; `ESC c` (reset) calls an empty `Reset()` (`tview.h:133`).
- **SGR:** 0, 1, 2, 4, 5, 7, 8, 30–37, 40–47 only. Missing: 3 (italic), 9 (strike), 21/22–29 (resets), 39/49 (default colours, which every modern app emits), 90–97/100–107 (bright), 38/48;5 (256), 38/48;2 (truecolor), 58/59 (underline colour), 4:x (underline styles). **Every SGR resets the pen first** (`m_nCurAttrib = g_nDefaultAttribs` at the top of the `m` case), so `ESC[1m` followed by `ESC[31m` loses the bold. SGR is meant to be cumulative; only 0 or an empty parameter resets.
- **DEC private modes:** only 1 (cursor keys), 5 (implemented wrongly as "set inverse attribute" on the current pen instead of reverse-video screen) and 47. Missing: 7 (autowrap), 25 (cursor visible), 6 (origin), 12 (blink), 1049/1047/1048, 1000/1002/1003/1006 (mouse), 1004 (focus), 2004 (bracketed paste), 2026 (synchronized output).
- **No replies.** No DA1/DA2 (`CSI c`, `CSI > c`), DSR/CPR (`CSI 5n`, `CSI 6n`), DECRQM or XTVERSION. Neovim, fish, readline's `enable-bracketed-paste` probe and many TUIs query and either wait or fall back.
- **Missing common sequences:** `CSI S`/`T` (scroll up/down), `X` (erase chars), `b` (repeat), `Z` (back-tab), `g`/`ESC H` (tab stops; tabs are hard-coded every 8 and emitted as spaces, so `\t` overwrites text), `CSI q` (cursor shape), `CSI ! p` (soft reset), `CSI t` (window ops, title stack), `ESC # 8`.
- **Character sets:** `ESC ( 0` is accepted and ignored; line drawing only works through SO/SI, mapped to private glyphs in the control range of the system font (`tview.cpp:1667-1706`) instead of U+2500 box-drawing characters.
- **Debug output** goes to `printf` per unknown sequence when `-d` is set, which is fine, but there is no way to log raw input to a file for test fixtures.

### 4.4 Rendering

- `Paint()` redraws everything; the 40 ms timer redraws dirty lines by comparing with a "previous" buffer. Output isn't coalesced with input, so latency is up to 40 ms and `cat` of a large file is slow.
- Bold is a double strike offset by 1 px; dim, blink and invisible are stored but not drawn; no italic font.
- The cursor is a 2-px I-beam only, with no block or underline shape and no unfocused style.
- The palette is a fixed 16-colour CGA-style table (`settings.cpp`), not configurable beyond the normal and selection colours.
- Glyphs come from the appserver's `DrawString` with the default fixed font; no fallback for characters the font lacks. (The `cairo` branch adds Pango/fontconfig, which solves this in Stage 6.)

### 4.5 Keyboard, mouse and clipboard

- **Backspace sends `^H`** (`tview.cpp:2839`) and the pty's `VERASE` is `^H` (`vfs/pty.c:1487`). Every current terminal sends DEL (0x7f); `xterm-256color` says `kbs=^?`. Changing it needs both ends changed together.
- **Home has no case**, so it sends Syllable's `VK_HOME` code `0x01` (Ctrl-A); End sends `0x05` (Ctrl-E) explicitly. That works in readline by luck and is wrong in vim, less and nano.
- No modifier encoding: Shift/Ctrl/Alt with arrows, Home/End, PgUp/PgDn and F-keys send the same as without (xterm sends `CSI 1;5C` etc.). Ctrl-Left/Right do nothing, Ctrl-Up/Down/PgUp/PgDn/Home/End scroll the view. Shift-Tab isn't `CSI Z`.
- F1–F4 send `CSI 11~`–`14~` (old rxvt/vt220); xterm sends `SS3 P`–`S`. Keypad application mode is stored (`m_nKeyPadMode`) but never used.
- Alt doesn't send an ESC prefix, so Alt-b/Alt-f in readline and Meta bindings in emacs/nano don't work.
- **Ctrl-C copies when there is a selection** and Ctrl-V always pastes, so the terminal steals `SIGINT` and readline's quoted-insert. Modern terminals use Ctrl-Shift-C/V or Super-C/V; your Super-key work (`keyboard/numlock-super-key-2026-11-03.md`) makes Super-C/V possible.
- Paste writes the text raw: no bracketed paste, and newlines go as `\n` rather than `\r`.
- No mouse reporting, no focus events, no right-click menu option to disable.
- The bell is ignored (no visual or audible bell).

### 4.6 Bugs to fix even before the rewrite

| Where | Bug |
|---|---|
| `tview.cpp:140` | 200-column line arrays; wider windows write out of bounds. |
| `tview.cpp:2520` | UTF-8 sequence split across reads reads past the buffer. |
| `tview.cpp:2016`, `:2312` | `argv[16]` has no bound check. |
| `main.cpp:553` | Kills the read thread twice; the update thread is never killed. |
| `main.cpp:567` | `printf` with `%i` and no argument. |
| `main.cpp:504` | Initial pty size is hard-coded 80×24 instead of the saved size, so the shell starts with the wrong size until the first resize. |
| `main.cpp:461-472` | Finds a pty by trying `open(O_CREAT)` up to 10,000 names. Should use `openpty`/`posix_openpt` once Stage 3 lands. |
| `main.cpp:286-300` | On a read error it sleeps 40 ms and retries forever; EOF (0) spins without sleeping. |
| `tview.cpp:2433` | Title buffer is a never-reset static (see 4.3). |
| `tview.cpp:2040` | `?5h` changes the pen, not the screen. |
| `tview.cpp:2189` | Every SGR resets the pen before applying, so attributes don't accumulate. |

## 5. Plan

Stages are ordered by dependency. 0–3 don't need the parser rewrite and can go in now; 4–9 build the new Terminal around your rewrite.

### Stage 0: quick fixes (small, now)

Shell and environment:
1. Stop setting `TERM` in `/etc/profile`. The Terminal sets it; sshd passes it through. Until Stage 5 lands, make the Terminal set `TERM=xterm-color` (8 colours, which the current parser does) or ship a small `syllable-term` terminfo entry that lists exactly what works today (see Stage 9).
2. Fix `TERMINFO` (remove it, or point it at `/system/resources/ncurses/5.7/share/terminfo`) and drop `COLORTERM`.
3. Remove `LESSCHARSET=latin1`; use `LESS=-R` so colour from git/ls passes through.
4. Remove `PATH=$PATH:./` from both `.profile` files; fix `PS1`.
5. `EDITOR=nano`; keep AEdit as `VISUAL` only when the desktop is running.
6. Make the setup-task loops glob-based and quiet.
7. Delete or regenerate the Builder's copy of `profile`.

Terminal (bug fixes only, no new features): the table in 4.6. These are each a few lines and make the current Terminal safe to use while the rewrite happens.

### Stage 1: environment structure and UTF-8 locale

1. Compile locales at image build time with the host's `localedef` for glibc 2.7 data, at least `C.UTF-8` and `en_US.UTF-8` (posix audit P2-4). glibc 2.7 has no built-in `C.UTF-8`; generate it from `share/i18n/locales/C` if present, else `en_US.UTF-8` is the default.
2. Default `LANG=en_US.UTF-8` in a system file the Preferences app can change (`/system/user-setup.sh` or `/etc/locale.conf`), with `LC_*` unset.
3. Split startup files the conventional way:
   - `/etc/profile`: environment only (PATH, DLL_PATH, locale, XDG, EDITOR), then sources `/etc/profile.d/*.sh`, then `~/.bashrc` if interactive.
   - `/etc/bash.bashrc` (compile bash with `-DSYS_BASHRC`, or source it from `/etc/profile` and `~/.bashrc`): prompt, aliases, history, `shopt`, title, completion.
   - `/etc/inputrc`: key bindings for the Terminal's sequences, `set enable-bracketed-paste on` (readline 8), `set completion-ignore-case on`, `set colored-stats on`, `set bell-style visible`.
   - Skeleton `~/.bashrc` and `~/.bash_profile` in `home/.template` that source the system files.
4. Set `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, `XDG_CACHE_HOME`, `XDG_STATE_HOME` to `~/.config` etc., and `XDG_RUNTIME_DIR` to a per-user dir in `/tmp` until there is a proper one.
5. Move the compiler variables behind `use-gcc` so a plain shell has a clean environment.
6. Set `umask 022` explicitly and `TZ` from the system clock settings.

### Stage 2: userland upgrades

Build with GCC 9.5 (all of these build with C99/C11). Order matters because of linkage:

| Port | From → to | Why |
|---|---|---|
| ncurses | 5.7 → 6.5, `--enable-widec`, shared | Wide-character curses for UTF-8 TUIs, current terminfo (`tmux-256color`, `xterm-direct`, accurate `xterm-256color`), `tinfo` split. |
| readline | 6.0 → 8.2 | Bracketed paste, better UTF-8 redisplay, `colored-completion-prefix`. |
| bash | 4.0 → 5.2 (or 5.3) with `--with-installed-readline` and `--with-curses` after ncurses | `checkwinsize` on by default, `EPOCHSECONDS`, `wait -n`, fixes; and it links terminfo instead of `/etc/termcap`. bash-completion 2.x needs ≥ 4.2. |
| less | 429 → 661+ | UTF-8 and SGR handling, OSC 8, mouse. |
| nano | 2.0.3 → 8.x (needs ncursesw) | UTF-8 editing, syntax colour. |
| coreutils | 6.4 → 9.x | UTF-8-aware tools, `ls` quoting, `dircolors` with current `LS_COLORS`. |
| grep / sed / gawk | 2.5.4 / 4.2.1 / 3.1.6 → 3.11 / 4.9 / 5.3 | grep 2.5 is very slow in UTF-8 locales; `--color=auto` defaults. |
| Optional | zsh 5.9, tmux 3.5, bash-completion 2.x, Neovim (already in progress) | tmux needs Stage 3 (`posix_openpt`) and libevent. |

Then retire `/etc/termcap` (keep it only if some old program still needs it) and generate `LS_COLORS` with `dircolors` instead of the hard-coded string.

### Stage 3: PTY and line discipline (kernel and libc)

1. Unix98 ptys: `/dev/ptmx` and `posix_openpt`/`grantpt`/`unlockpt`/`ptsname`, plus `TIOCSCTTY`, keeping `/dev/pty/mst` for old programs (posix audit P0-3). Needed by tmux, Python's `pty`, Neovim `:terminal`, and modern OpenSSH's pty allocation.
2. `IUTF8` in `c_iflag`, and make canonical-mode ERASE/WERASE remove whole UTF-8 characters (`vfs/pty.c:926`, `:1020`).
3. Change the default `VERASE` to DEL (0x7f) together with the Terminal's Backspace (Stage 7). Check sshd's pty setup uses the same default.
4. Confirm `TIOCSWINSZ` delivers `SIGWINCH` to the foreground process group (it does, `vfs/pty.c:1743`) and that the Terminal sets the size before exec.

### Stage 4: Terminal architecture (before or with the rewrite)

Split `TermView` into pieces with no GUI dependency in the core, so the core builds and tests on the Mac:

| Component | Job | Depends on libsyllable? |
|---|---|---|
| `Parser` | Bytes in, actions out (print, execute, csi_dispatch, esc_dispatch, osc, dcs) | No |
| `Screen` / `Grid` | Cells, cursor, modes, tab stops, scroll regions, primary and alternate buffers, scrollback ring | No |
| `Terminal` | Glue: applies parser actions to the screen, queues replies, tracks title/cwd/hyperlinks | No |
| `InputEncoder` | Key + modifiers + modes → bytes (Stage 7) | No (takes plain key codes) |
| `PtyHost` | Open pty, fork/exec, resize, read loop | Thin POSIX |
| `TermView` | Paint the grid, selection, mouse, clipboard | Yes |

Threading: the read thread parses into the model under a model mutex (not the window lock) and posts one "damaged" message to the view; the view repaints damaged rows on its own thread and coalesces to the display rate. That removes the 40 ms poll. Replies (CPR, DA) go back to the pty from the read thread through a queue.

Make one process own several windows (and tabs later) by turning the globals into a per-session object.

### Stage 5: line-parsing rewrite (Rick, after Cairo)

Your stage. Section 6 is the specification.

### Stage 6: rendering on Cairo/Pango

Assumes the `cairo` branch's `GFX_*`/Pango text path lands (see appserver review 7.1; it currently needs a 32-bit framebuffer).

1. Draw runs of cells with the same style in one Pango call, positioned on the cell grid (don't let Pango kern across cells).
2. Real bold/italic/bold-italic faces, falling back to synthetic bold only when the face is missing.
3. Font fallback through fontconfig for glyphs the main font lacks (CJK, symbols, emoji); wide glyphs centred in two cells.
4. Draw box-drawing (U+2500–257F), block elements (U+2580–259F) and Powerline/Braille glyphs procedurally so they join seamlessly at any font size.
5. Colour: 256-colour xterm palette plus RGB per cell; configurable 16-colour base palette; default fg/bg/cursor/selection colours; optional "bold is bright".
6. Underline styles (single, double, curly, dotted, dashed) and underline colour; strikethrough; overline; dim as alpha.
7. Cursor shapes from `DECSCUSR` (block, underline, bar; blinking or steady) and a hollow block when unfocused.
8. Damage-driven repaint of changed rows only; honour synchronized output (mode 2026) by holding repaint until the end marker or a timeout.

### Stage 7: input encoding

1. Backspace = DEL (with Stage 3); Ctrl-Backspace = `^H` (or `ESC DEL` with Alt).
2. Home/End: `CSI H`/`CSI F`, or `SS3 H`/`SS3 F` in cursor-key application mode. Insert/Delete/PgUp/PgDn as now.
3. xterm modifier encoding for cursor, editing and function keys (`CSI 1;<mod>X`, `CSI <n>;<mod>~`), F1–F4 as `SS3 P`–`S`, keypad application mode (DECKPAM).
4. Alt sends an ESC prefix (configurable, because the appserver uses Alt for shortcuts).
5. Shift-Tab = `CSI Z`.
6. Optional later: `modifyOtherKeys` level 2 and the kitty keyboard protocol (`CSI > 1 u` etc.), which Neovim and fish use when offered.
7. Move Copy/Paste to Super-C/V (or Ctrl-Shift-C/V) and let Ctrl-C/Ctrl-V through. Keep Shift-Insert paste.
8. Bracketed paste (mode 2004), converting `\n` to `\r`, and stripping `ESC[201~` from pasted text so a paste can't inject commands.
9. Mouse reporting: X10/normal/button/any-event (1000/1002/1003) with SGR encoding (1006); Shift bypasses it for selection. Focus in/out (1004). Wheel sends arrow keys in the alternate screen when mouse reporting is off ("alternate scroll", mode 1007).

### Stage 8: Terminal features

Scrollback as a ring buffer with a configurable size (10,000+ default) and a "clear scrollback" command; reflow on resize using the soft-wrap flag; find in scrollback; clickable OSC 8 hyperlinks and detected URLs; visual bell and an urgency hint on the window/Dock; profiles (font, palette, size) in Syllable settings; "new window here" using the OSC 7 working directory; shell-integration marks (OSC 133) for jumping between prompts.

### Stage 9: terminfo and TERM

1. While the parser is the old one: ship a terminfo entry that describes it honestly (Stage 0), so programs don't send what it can't handle.
2. After Stages 5–7 pass the test suite: the Terminal sets `TERM=xterm-256color` and `COLORTERM=truecolor` itself, for compatibility with remote hosts that don't know a Syllable entry.
3. Optionally also ship a precise entry (say `pyro-terminal`, built `use=xterm-256color` plus `Tc`/`RGB`, `Smulx`, `Setulc`, `Ss`/`Se`, `Sync`, `BE`/`BD`/`PS`/`PE` for bracketed paste, `XM` for mouse) and install it with `tic -x`. Answer XTGETTCAP (`DCS + q`) from it, so Neovim and tmux can query capabilities even over SSH.
4. Add the Ghostty entry to the image's terminfo, so `ssh` from your Mac with `TERM=xterm-ghostty` works without falling back.

## 6. Stage 5 specification: what the line-parsing rewrite should cover

### 6.1 Parser structure

- Implement the DEC/ANSI state machine from Paul Williams' "A parser for DEC's ANSI-compatible video terminals" (vt100.net/emulators). States: ground, escape, escape_intermediate, csi_entry, csi_param, csi_intermediate, csi_ignore, dcs_entry, dcs_param, dcs_intermediate, dcs_passthrough, dcs_ignore, osc_string, sos_pm_apc_string. It's table-driven, byte-at-a-time, and keeps its state across `read()` calls, which fixes the split-sequence problems by construction.
- **C0 controls execute in any state** except inside strings; CAN (0x18) and SUB (0x1A) abort a sequence; ESC restarts one.
- **Parameters:** at least 16 (32 is safer), each clamped (e.g. to 65535); an empty parameter means "default", which differs per command (0 for ED/EL/SGR, 1 for cursor moves); colon subparameters stored per parameter, for SGR 38/48/58 and underline styles; private markers `?`, `>`, `=`, `<`; up to two intermediate bytes. Overflowing sequences go to csi_ignore and are dropped whole.
- **Dispatch key** = (private marker, intermediates, final byte), so `CSI ? 25 h`, `CSI > c`, `CSI SP q` and `CSI ! p` never collide.
- **Strings (OSC/DCS/APC/PM/SOS):** terminated by BEL or ST (`ESC \`), and by 8-bit ST only in non-UTF-8 mode; a length cap (e.g. 64 KB for OSC 52, 4 KB otherwise) after which the string is ignored rather than truncated and acted on.
- **UTF-8:** a streaming decoder in front of the ground state only (control and escape bytes are ASCII). Malformed or overlong sequences and surrogates become U+FFFD. C1 controls (0x80–0x9F) aren't interpreted in UTF-8 mode. Keep an optional legacy mode for ISO-8859-1 if wanted.
- **Fast path:** in ground state, scan a run of printable ASCII and hand it to the screen in one call; that's most of the throughput for `cat` and compiler output.
- Keep the parser free of GUI code and allocation in the hot path, so it can be unit-tested on the Mac and fuzzed (libFuzzer on the host, with ASan).

### 6.2 Screen semantics the parser drives

- **Cell:** code point, a small index into a combining-character table (or a grapheme cluster string), width (1, 2, or 0 for the right half of a wide char), foreground and background as tagged colours (default / indexed 0–255 / RGB), underline colour, flags (bold, dim, italic, underline style, blink, inverse, invisible, strikethrough, overline), hyperlink id.
- **Widths:** `wcwidth` from current Unicode tables generated at build time (glibc 2.7's are from 2007), with East Asian Wide/Fullwidth as 2, combining marks and ZWJ as 0, emoji presentation as 2. Writing a wide char in the last column wraps first; overwriting half of a wide char clears the other half.
- **Autowrap (DECAWM, mode 7)** with the deferred "pending wrap" flag at the right margin, and a soft-wrap flag on the line.
- **Origin mode (DECOM, 6)**, scroll regions (DECSTBM) with correct clamping, cursor moves clamped to the region where the spec says.
- **Tab stops:** default every 8, HTS/TBC/CHT/CBT; `\t` moves the cursor without writing spaces.
- **Alternate screen:** 47, 1047 (clear on exit), 1048 (save cursor), 1049 (save cursor + switch + clear); the alternate buffer has no scrollback.
- **Erase uses the current background colour** (BCE), which `xterm-256color` declares.
- **Character sets:** G0/G1 designation (`ESC ( 0`, `ESC ) 0`, `ESC ( B`), SO/SI, single shifts optional; DEC Special Graphics mapped to real U+2500 code points.
- **Save/restore** (DECSC/DECRC, `CSI s`/`CSI u` when left/right margins are off): cursor position, pen, origin mode, charset state, pending-wrap.
- **Resets:** RIS (`ESC c`) full, DECSTR (`CSI ! p`) soft.

### 6.3 Sequences to support

C0: BEL, BS, HT, LF, VT, FF, CR, SO, SI.

ESC: IND (`D`), NEL (`E`), HTS (`H`), RI (`M`), DECSC (`7`), DECRC (`8`), DECKPAM (`=`), DECKPNM (`>`), RIS (`c`), DECALN (`# 8`), SCS (`( ) * +` with final), ST (`\`).

CSI:

| Group | Sequences |
|---|---|
| Cursor | CUU `A`, CUD `B`, CUF `C`, CUB `D`, CNL `E`, CPL `F`, CHA `G`, CUP `H`, HVP `f`, VPA `d`, VPR `e`, HPA `` ` ``, HPR `a`, CHT `I`, CBT `Z` |
| Erase / edit | ED `J` (0,1,2,3), EL `K` (0,1,2), ECH `X`, ICH `@`, DCH `P`, IL `L`, DL `M`, REP `b` |
| Scroll | SU `S`, SD `T`, DECSTBM `r` |
| Tabs | TBC `g` (0, 3) |
| Attributes | SGR `m`: 0, 1, 2, 3, 4 (and 4:0–4:5), 5, 7, 8, 9, 21, 22, 23, 24, 25, 27, 28, 29, 30–37, 38;5;n, 38;2;r;g;b and the colon forms, 39, 40–47, 48 (same forms), 49, 53, 55, 58, 59, 90–97, 100–107 |
| Modes | SM/RM `h`/`l`: 4 (IRM), 20 (LNM). DECSET/DECRST `? h`/`? l`: 1, 5, 6, 7, 12, 25, 47, 66, 1000, 1002, 1003, 1004, 1005 (ignore), 1006, 1007, 1047, 1048, 1049, 2004, 2026 |
| Reports | DA1 `c`, DA2 `> c`, DSR `5 n`, CPR `6 n`, DECXCPR `? 6 n`, DECRQM `$ p` and `? $ p`, XTVERSION `> q` |
| Other | DECSCUSR `SP q`, DECSTR `! p`, XTWINOPS `t` (8 resize: ignore or honour; 14/16/18 size reports; 22/23 title stack), `CSI ? u` reply 0 if kitty keyboard is not implemented |

OSC: 0, 1, 2 (titles, UTF-8), 4 (set/query palette), 7 (working directory, `file://host/path`), 8 (hyperlinks), 10/11/12 (query/set fg, bg, cursor colour), 52 (clipboard: setting allowed, reading off by default for security), 104/110/111/112 (resets), 133 (prompt marks). Unknown OSCs are consumed silently.

DCS: DECRQSS (`$ q`) for SGR and DECSCUSR at least; XTGETTCAP (`+ q`) answered from the shipped terminfo entry; anything else (sixel, tmux passthrough) consumed and ignored for now. APC/PM/SOS consumed and ignored (the kitty graphics protocol can come later).

Replies are queued and written to the pty after the current buffer is processed, never from inside the screen lock.

### 6.4 Tests

- **Unit tests on the host:** byte streams in, expected grid and cursor out. Cover splitting every test input at every byte boundary to prove the parser is resumable.
- **Recorded fixtures:** add a `--record FILE` option to save raw pty output, then replay sessions of vim, less, nano, htop-style tools and `git log` as regression tests.
- **vttest** (Thomas Dickey) menus 1, 2, 3, 6 and 8, and **esctest** (George Nachman, used by iTerm2 and xterm) run against the Terminal; aim for the same pass list as xterm for the sequences in 6.3.
- **Real programs:** Neovim `:checkhealth`, tmux (after Stage 3), `less -R`, nano 8, `git log --graph --color`, a truecolor gradient script, `printf` tests for wide characters and combining marks, and resizing during output.
- **Fuzzing** the parser with libFuzzer + ASan on the Mac.
- **Throughput:** time `cat` of a 50 MB log before and after; target within a few times of xterm.

### 6.5 Suggested order inside Stage 5

1. State machine and UTF-8 decoder with unit tests (no screen changes).
2. Port the existing actions onto the new dispatch and delete `AddEscCode`/`ParseBraceEsc`/`ParseParanEsc`.
3. New cell format and colours (SGR complete).
4. Modes, alternate screen, autowrap, tabs, widths.
5. Replies, OSC, DCS.
6. vttest/esctest pass, then switch `TERM` (Stage 9).

## Appendix A: checks to run on the box

Run these in a Terminal window (I didn't connect to the box):

```sh
bash --version | head -1
ldd /bin/bash 2>/dev/null; strings /bin/bash | grep -E '^/etc/termcap|terminfo' | head   # which terminal library bash uses
echo "$TERM"; infocmp | head -3; tput colors              # expect xterm-r6 and -1 today
ls /usr/share/terminfo 2>&1 | head -1                      # TERMINFO target missing?
locale -a; locale                                          # expect only C/POSIX
stty -a | head -3                                          # erase = ^H, no iutf8
```

## Appendix B: files referred to

- `system/image/etc/profile`, `system/image/system/user-setup.sh`, `system/image/home/{root,.template}/.profile`, `system/image/etc/passwd`
- `system/other/config/etc/termcap`
- `cmake/Ports.cmake`, `cmake/ports/bash.cache`
- `system/apps/utils/aterm/{main.cpp,tview.cpp,tview.h,settings.cpp}`
- `system/sys/kernel/kernel/vfs/pty.c`
- `system/sys/include/syllable/gui/guidefines.h` (VK_ codes)
- Project docs: `docs/posix-audit-2026-10-30.md` (P0-3, P2-4), `appserver-review-20261104T120736-ef3a.md` (Section 7.1, cairo branch), `keyboard/numlock-super-key-2026-11-03.md`
