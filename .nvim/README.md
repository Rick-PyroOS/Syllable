# Neovim for Syllable (Pyro)

CLion's C/C++ workflow in Neovim: its keymap, Find Action and Search Everywhere, Show Usages,
quick documentation with clickable links, parameter info, errors in the margin, editor tabs,
the project tree and background code checks. Built on clangd.

## Install (macOS or Linux)

From the Syllable checkout:

```sh
.nvim/install.sh
```

It installs what the config needs (Neovim 0.11+, ripgrep, fd, the tree-sitter CLI, a C
compiler, cppcheck, semgrep, the JetBrains Mono Nerd Font), links `~/.config/nvim` to
this folder (an existing config is moved to `~/.config/nvim.backup-<date>` first), and
installs the plugins, clangd and the parsers. On macOS it also sets up the iTerm2
profile, the Karabiner Shift Shift rule and the Raycast extension's packages when those
apps are installed. It's safe to run again; `.nvim/install.sh --help` lists the options
(`--dry-run` shows what it would do). macOS needs Homebrew; Linux uses apt, dnf, pacman
or zypper (with sudo) and puts anything else in `~/.local`.

Claude integration also needs the Claude Code CLI (`claude`) on your PATH. For the local
model checks (the `M` in the margin), add `--local-model`: it installs Ollama and a
coding model sized for the machine's memory, on macOS or Linux (`--model NAME` picks the
model yourself). It's off by default because the model is several GB; see `../local-ai.md`.

`extras/` holds the iTerm2 profile generator and the Karabiner rule; `../.raycast` is the
Raycast extension. `../.clangd` configures clangd for the tree.

Health checks: `:checkhealth`, `:Lazy`, `:Mason`, `:LspInfo` (or `:checkhealth vim.lsp`).

## If Cmd keys or Shift Shift do nothing

If `Cmd+Shift+P` opens iTerm's Page Setup (or any Cmd key opens an iTerm menu), the window
isn't using the **Neovim (CLion keys)** profile, so the keys never reach Neovim. Fix it once:

1. Install the profile:

   ```sh
   python3 .nvim/extras/iterm2/make_profile.py --parent Default \
     --out ~/Library/Application\ Support/iTerm2/DynamicProfiles/neovim-clion-keys.json
   ```

2. iTerm2 > Settings > Profiles: select **Neovim (CLion keys)**, then Other Actions > Set
   as Default.
3. Open a **new** iTerm window and start Neovim there (open windows keep their old profile).
   Session > Edit Session shows which profile a window uses.

If Shift Shift does nothing, it also needs the Karabiner rule (and the profile above):

```sh
mkdir -p ~/.config/karabiner/assets/complex_modifications
cp .nvim/extras/karabiner/neovim-shift-shift.json ~/.config/karabiner/assets/complex_modifications/
```

then Karabiner-Elements > Complex Modifications > Add predefined rule > "Neovim: Shift
Shift opens Find" > Enable.

Without any setup, the Space menus always work: `Space` then wait shows every command, and
`Space ?` lists all keys.

## Daily keys (leader = Space; press Space and wait for the menu)

| Keys | What |
|---|---|
| `Space ?` | **keymap cheat sheet popup** (every shortcut this config sets; `q` closes) |
| `Cmd+F` | **Find**, like CLion's double Shift (Shift Shift works too with the Karabiner rule below, also while typing): tabs All / Classes / Files / Symbols / Text. `Tab` switches tab, `Alt+N` adds non-project items, `Shift+Enter` opens in a right split. Works in the project tree too |
| `Space Space` | **Command palette**: every shortcut and `:` command by name; Enter runs it, recently run ones first (also `Space p`, `Cmd+Shift+A`, `Cmd+Shift+P`; the Cmd keys work while typing too) |
| `Space ff` | find file |
| `Space fg` / `Space fw` | grep / grep word under cursor |
| `Space e` | project tree (opens on the left at startup in a project; click a file to open it, a folder to expand it; `Esc` goes back to the file and leaves the tree open, `q` or `Space e` closes it). Right-click a file or folder for Open, New, Rename, Delete, Duplicate, Copy Path, Reveal in Finder, Find in Folder, Terminal Here and Collapse All. A file with problems shows a red ✗ (or yellow !) at the end of its row; right-click > Show Problems lists them. Folders don't show their files' problems. Folders have ▸ / ▾ arrows; collapsing a folder also collapses the folders inside it. Drag the line between the tree and the file to resize it; the width is remembered |
| `:q`, `:wq`, `:x`, `Space q` | quit Neovim from any window; asks Save all / Discard / Cancel if anything is unsaved (`:q!` quits without asking) |
| `:close` or `Ctrl-w c` | close just one split |
| `Shift-h` / `Shift-l` | previous / next tab (open file); click a tab to switch, rest the mouse on it to see its full path (click the path to copy it), scroll over the tabs (up/down or sideways) to scroll them, right-click a tab for Close, Close Others / Left / Right / All, Open to the Right, Copy Path, Copy Relative Path, Show in Project Tree and Reveal in Finder, middle-click to close |
| `Space 1`..`9`, `Space bp` | jump to tab N / pick a tab by letter |
| `Space bd`, `Space bo` | close tab / close all other tabs |
| `gd` `gD` `gy` | definition / declaration / type |
| `Ctrl`+click | open header under mouse / go to definition (`Ctrl-o` goes back); on a declaration or definition, show usages |
| `grr` `gri` | references / implementations |
| `Space cu` | show usages: a small list under the name of everywhere it's used, in three columns (file and line, folder, code; a long folder is cut short, rest the mouse on it for the whole path); type to filter, Enter or a click jumps, Esc closes |
| `K` | hover docs |
| `grn` or `Space cr` | rename |
| `Space o` | switch .c/.cpp <-> .h |
| `[d` `]d`, `Space cd` | prev/next diagnostic, show line diagnostic |
| `Ctrl-Space`, `Ctrl-n/p`, `Enter` | open / move / accept completion (`Ctrl-e` closes it). Accepting a function inserts `foo()` with the cursor inside and the parameters shown above it, and adds the `;` when the call ends the statement |
| `Space cf` | format file, or the selection in visual mode (tool picked by language) |
| `:enew`, `:set ft=cpp`, `Space cf` | format a scratch buffer (your project's style even outside it, with `vim.g.clang_format_fallback`) |
| `Space tf` | toggle format on save (the lines you changed, when you save with `Cmd+S` or `:w`; auto-save doesn't format) |
| `Space ta` | toggle auto-save |
| `Space tw` | toggle word wrap (off by default; remembered across restarts) |
| `Space tz` | toggle whether `Ctrl+Z` suspends Neovim back to the shell (off by default, so `Ctrl+Z` does nothing; remembered) |
| `Space td` | toggle double-click selecting the word, triple-click the line (off by default, so extra clicks just place the cursor; remembered) |
| `Space tk` | toggle the popup that lists the keys after `Space`, `"` or `'` (off by default; `Space Space` works either way; remembered) |
| `Space tq` | toggle quick documentation on mouse hover, like CLion: rest the mouse on a name for half a second and clangd's type and docs pop up beside it; move off the name to close it, or onto the popup to scroll it (on by default; remembered; `F1` / `K` show it for the cursor either way) |
| `Space tx` | toggle the sticky header, like CLion's: the class and function you're in stay pinned at the top of the editor while you scroll (on by default; remembered) |
| `Space tc` | toggle cppcheck scanning (remembered across restarts) |
| `Space ts` | toggle semgrep scanning (remembered across restarts) |
| `Space tm` | toggle the local model checks (the `M` in the margin; remembered across restarts) |
| `Space cm` | run the local model checks on this file now |
| `Space tb` | show / hide the status bar (hidden by default; line and column stay at the bottom right) |
| `Space te` | toggle whether the project tree remembers which folders were open (remembered across restarts) |
| `Space tl` | toggle whether the project tree follows the file you're editing, like CLion's "Always Select Opened File": switch tabs and the tree opens the folders down to that file and highlights it (on by default) |
| `Space tT` | Syllable only: toggle the thread (mutex) check in the margin (remembered) |
| `Space tn` | toggle the notification balloon (off: notifications are corner toasts; remembered) |
| `Space t/` | toggle the find bar for `/` and `Cmd+F` (off: plain Vim search; remembered) |
| `Space tI` | toggle showing images in the editor (off: they open in Preview; remembered) |
| `Space cx` | Syllable only: which threads run this? Explains, for the function under the cursor, which threads can reach it and whether a lock is held |
| `Space cX` | Syllable only: run the thread check now. The check knows nothing about any framework by itself: Syllable's `.nvim.lua` describes its threads, loopers, views and locks in `vim.g.thread_model` and then calls `require("config.threads").setup()`. Another project can turn it on the same way, with its own `vim.g.thread_model` in its `.nvim.lua` |
| `Space mp` | in a Markdown file: live preview in the browser, Mermaid diagrams included |
| `Space cp` | in a C or C++ file: doxygen preview of that file's docs in the browser, refreshed on each save (`:DoxygenStop` ends it; needs `doxygen`, which install.sh installs) |
| `Space tv` | in a CSV or TSV file: toggle the table view (columns lined up, header kept at the top; on when the file opens). Tab / Shift-Tab move between fields |
| `]h` `[h`, `Space gp` | next/prev git hunk, preview hunk |
| `Space gh` | Local History of this file, like CLion's: every save (auto-save too) is kept for 30 days in one SQLite database (`~/.local/state/nvim/local_history.sqlite3`), separate from git. Newest first, with a diff against now; Enter compares side by side, `Ctrl+R` restores that version (one undo puts it back) |
| `Space gH` | Local History of the project: every file's changes newest first (saves a few minutes apart are one row), files you have since deleted (Enter opens read-only, `Ctrl+R` writes it back) and labels. Right-click a folder in the project tree > Local History for just that folder |
| `Space gL` | put a label on this moment in Local History ("before the refactor"; also `Ctrl+L` in the lists) |

Markdown files show headings, lists, tables and code blocks formatted in the editor (the
line under the cursor shows the raw text). Mermaid diagrams can't be drawn in a terminal,
so they show in the browser preview (`Space mp`). That's `lua/plugins/markdown.lua`.

Files, the project tree and popups you can scroll (`K`'s docs, the error popup) show a thin
scrollbar at their right edge whenever there's more than fits (word wrap included), and files
and the tree show one at the bottom when a line is wider than the window, parameter-name hints
included (hidden while word wrap is on); drag either to scroll. The project tree has no cursor;
the highlighted row shows where you are. Right-click in a file for Show Usages (under Go to definition) and Delete Line (Delete Lines
with a selection); Neovim's Inspect entry is removed. Switching tabs doesn't move the tab bar
unless the tab you went to is out of view. The cursor is an orange block (insert mode blinks).
Right-click menus need iTerm2 > Settings > Pointer > **Right click reported to apps, does
not open menu** turned on; otherwise iTerm shows its own menu and Neovim never sees the click.

The mouse pointer is a text beam only over the code of the pane you're typing in, and an
arrow everywhere else (the tab bar, the project tree, other panes, the margin, the
scrollbars), with a resize arrow over the line between two panes (drag it to resize). Neovim sets it with the OSC 22 escape code, which needs a recent iTerm2; an
older one ignores it and keeps the text beam (`lua/config/pointer.lua`).

Opening a binary file (an object file, an executable, an image) asks first; Cancel doesn't
read it (`lua/config/binary.lua`).

## CLion shortcuts (macOS keymap)

The usual CLion keys work too (`lua/config/clion_keys.lua`). iTerm2 keeps Cmd shortcuts for
its own menus, so Cmd keys need the **Neovim (CLion keys)** iTerm profile, once:

```sh
python3 .nvim/extras/iterm2/make_profile.py --parent Default \
  --out ~/Library/Application\ Support/iTerm2/DynamicProfiles/neovim-clion-keys.json
```

(`install.sh` does this; `--iterm-parent NAME` picks another profile to inherit from.)
It appears in iTerm2 > Settings > Profiles, inherits colors and font from that
profile, and only adds key mappings. Select it, then Other Actions > Set as Default (or
open a window with it). Cmd+C/V/T/N/Q are left to iTerm. F-keys need `fn` unless macOS
is set to use F1, F2... as standard function keys. In Neovide, Cmd keys work with no setup.

**Shift Shift** needs [Karabiner-Elements](https://karabiner-elements.pqrs.org), because a
terminal never tells Neovim that Shift alone was pressed. (Kitty's "report all keys" mode
would, but Neovim then reads Shift+1 as `1` instead of `!`, so typing breaks.) The rule
turns a double tap of either Shift into F19, only while iTerm2 is in front; the profile
above passes F19 to Neovim, which opens Find. Install it once:

```sh
mkdir -p ~/.config/karabiner/assets/complex_modifications
cp .nvim/extras/karabiner/neovim-shift-shift.json ~/.config/karabiner/assets/complex_modifications/
```

then Karabiner-Elements > Complex Modifications > Add predefined rule > "Neovim: Shift
Shift opens Find" > Enable. The taps must be within 0.4 s with no other key between.

| Keys | What |
|---|---|
| `Cmd+O` / `Cmd+Shift+O` / `Cmd+Alt+O` | Find on Classes / Files / Symbols (press again for non-project items) |
| `Cmd+E`, `Cmd+Shift+A` / `Cmd+Shift+P` | recent files, command palette (every shortcut and command) |
| `Cmd+B`, `Cmd+Alt+B` | go to declaration, go to implementation (a function that's only declared offers to create its body in the matching .cpp, like CLion's Create definition) |
| `Alt+F7`, `Cmd+Alt+F7` | find usages (big list with a preview), show usages (small popup under the name) |
| `Ctrl+Cmd+Up` | switch header/source |
| `Cmd+[` / `Cmd+]` | back / forward |
| `Cmd+L`, `Cmd+F12` | go to line, file structure |
| `F2` / `Shift+F2` | next / previous error (across files after a build) |
| `Cmd+Shift+E` | recent locations: where you were reading or editing, newest first; `Cmd+Shift+E` again shows changed places only (`Space fl`) |
| `F3` / `Cmd+F3` | toggle a bookmark on the line / show bookmarks (`Space fB`) |
| `Cmd+1`, `Alt+F12` | project tree, terminal |
| `Cmd+W`, `Cmd+Shift+[` / `]` | close tab, previous / next tab |
| `Cmd+F`, `Cmd+R`, `Cmd+Shift+F` | Find (`/` searches the file), replace in file, Find on Text |
| `Cmd+Shift+R` | replace in files (the word under the cursor, or the selection, prefilled) |
| `Ctrl+Alt+H` | call hierarchy: a popup of every function that calls the one under the cursor, with a preview; Enter jumps (`Space ci`; `Space co` lists what it calls) |
| click the `▾` / `▸` left of the signs | fold or unfold that function, class or block, like CLion (`za` at the cursor; files open unfolded) |
| `Cmd+S`, `Cmd+Z`, `Cmd+Shift+Z`, `Cmd+A` | save all, undo, redo, select all |
| `Cmd+/` | comment line / selection |
| `Cmd+D`, `Cmd+Backspace` | duplicate line, delete line |
| `Alt+Shift+Up/Down`, `Cmd+Shift+Up/Down` | move line / selection |
| `Cmd+Alt+L` | reformat code |
| `Ctrl+G` / `Ctrl+Alt+G` | multi-cursor: add a caret on the next occurrence / on all occurrences of the word or selection; typing edits them all, `Esc` ends |
| `Ctrl+Alt+O` | optimize #includes: removes the unused ones and adds the missing ones clangd reports, after showing the list (`Space cI`) |
| `Cmd+Alt+T` | surround with if / while / for / try / `{ }` / `#ifdef` ... (`Space cs`) |
| `Cmd+J` | insert a live template |
| `Cmd+F9`, `Cmd+Shift+F9`, `Cmd+F2` | build in the background (`Space rb`), recompile this file (`Space rf`), stop the build (`Space rs`); `Space ro` shows / hides the Build panel |
| `Cmd+,` | settings: every toggle in one list |
| `Alt+Enter` | intention actions (code actions) |
| `Shift+F6`, `Cmd+P`, `F1` | rename, parameter info, quick documentation (press `Cmd+P` again, also while typing, or `F1` again to go into the popup and select and copy with `y`; `q` goes back). Underlined names in these popups are links: click one, or press Enter or `Ctrl-]` on it, to go to its definition (`Ctrl-o` comes back; several matches give a list) |
| `Cmd+Shift+/` | keymap cheat sheet (same as `Space ?`) |

Not mapped (no equivalent yet): `Cmd+N` generate,
run/debug keys. Cmd+click is iTerm's; use Ctrl+click (on a use of a name it goes to the definition; on the
name where it's declared or defined it shows its usages).

## Ghostty

[Ghostty](https://ghostty.org) works too, with less setup than iTerm2. Point
`~/.config/ghostty/config` at this folder's settings by adding the line

```
config-file = ~/.config/nvim/extras/ghostty/config
```

(`install.sh` adds it when Ghostty is installed). That sets `macos-option-as-alt`, so
Alt shortcuts reach Neovim instead of typing special characters, and passes the Cmd keys
through. Ghostty draws real images, so images open in the editor pixel for pixel (iTerm2
gets a chafa preview instead). Shift Shift works in Ghostty with the Karabiner rule above,
or without Karabiner via Hammerspoon (`extras/hammerspoon/shift_shift.lua`; `install.sh`
loads it from `~/.hammerspoon/init.lua`).

## Claude

| Keys | What |
|---|---|
| `Space ac` | open/close Claude in a right split |
| `Space as` (visual) | send selection to Claude |
| `Space ab` | add current file to Claude's context |
| `Space aa` / `Space ad` | accept / reject Claude's proposed diff |
| `Esc Esc` | leave the terminal back to normal mode |

Claude connects to Neovim over a local MCP bridge, so it sees your open file and
selection and shows edits as diffs. Running `claude` in a separate terminal works too:
type `/ide` and pick Neovim.

## Raycast

`Syllable/.raycast` is a Raycast extension with three commands: **Search Neovim
Actions** (every mapping with a description, plus `:` commands), **Switch Neovim Tab**
and **Open File in Neovim**. Enter runs or opens the item, Shift+Enter opens a file in a
right split, then iTerm comes to the front. Commands that take arguments (`:write`,
`:saveas`, `:edit`, ...) ask for them, and typing `:` then any command line
(`:w notes.txt`, `:%s/foo/bar/g`) runs it as if typed in Neovim.
Install once (needs Node 22.22 or newer):

```sh
cd .raycast && npm install && npm run dev     # install.sh runs the npm install
```

Each Neovim listens on `~/.local/state/nvim/raycast/<pid>.sock` (`lua/config/raycast.lua`).
With several open, Raycast shows a dropdown and picks the one you used last.

## Choices made on purpose

- **Format on save touches only the lines you changed**, for C, C++ and Lua: your git
  hunks, when you save with `Cmd+S` or `:w`. Nothing is formatted while you type, on `Esc`,
  or by auto-save. Old code around your edit is left alone, so diffs stay clean. Files
  outside git are never formatted on save. `Space tf` toggles it; `Space cf` formats a whole file or selection.
  C/C++ style is `Syllable/.clang-format`, tuned to match the existing tree as closely as it can.
- **clangd adds `#include`s on completion.** Items marked with a dot add one when accepted.
  If one lands in the wrong spot (include order here is fragile), move it or undo with `u`.
  To turn it off, set `--header-insertion=never` in `lua/config/lsp.lua`.
- **Tabs, width 4** by default; guess-indent switches per file when a file differs.
- Files changed outside Neovim (Claude, git, rsync) reload automatically.
- **Auto-save is on.** A file is written a second after you stop editing (after `Esc`, or a
  normal-mode change), when you switch to another tab, and when iTerm loses focus. Never
  while you're mid-typing, and never scratch buffers or read-only files. Auto-save doesn't
  format; only your own saves do. `Space ta` toggles it for the session.
- **`:q` quits everything**, not just the window you're in, after asking about unsaved
  files. With auto-save on there's rarely anything to ask about.

## clangd and the Syllable tree

`Syllable/compile_commands.json` gives clangd each file's real flags, read from its
Makefile (`-I.`, `-I../../server`, freetype, ...). Regenerate it after changing
Makefile include paths or adding directories:

```sh
python3 tools/gen_compile_commands.py     # from the Syllable checkout
```

It also lists box include folders that Makefiles use but `.sysroot/` doesn't have yet.
Reopen Neovim afterwards so clangd picks it up.

Config lives in the Syllable repo: `.clangd`, plus the shared flags the generator starts
from, `tools/base_flags.txt` (userland) and `tools/base_flags_kernel.txt` (kernel + drivers).
clangd itself only reads `compile_commands.json`; a new file borrows a neighbour's flags.

How it all fits together (header order, the box sync, the base flags) is written up in
`Syllable/tools/README.md`. To refresh the box's headers after installing libraries there:

```sh
python3 tools/gen_compile_commands.py --sync
```

Syllable's own editor settings (its file types, the clang-format style for scratch buffers, the
errno.h the local model reads, the thread check's model) are in `Syllable/.nvim.lua`. Neovim
runs it after this config when you start in the checkout, and asks once whether to trust it.

Known: some kernel files show a few errors at the top (`index_info` redefinition,
`nlink_t`/`sigset_t`/`fd_set` clashes). Those are real conflicts between the kernel
headers and the libc headers in the tree, not editor problems.

Doc popups (mouse hover, completion and `K`) tidy the Syllable sources' doxygen: `\par Description:`,
`\param`, `\return`, `\sa`, `\author` become headings and lists, and the `*****` line that
ends each comment is dropped. That's `lua/config/doxygen.lua`; it works with any clangd.
Syllable/.clangd also turns on clangd's own doxygen layout (`Documentation: CommentFormat: Doxygen`),
which puts each parameter's type next to its description and adds Brief and Returns sections.
It needs clangd 22 or newer: Neovim warns once if clangd is older, and `:MasonToolsUpdate`
updates it. Keep a name on every `\param`: clangd 22 and 23 crash when you hover a function
whose comment has an empty `\param` line.

Errors show in the margin plus an underline. The margin has three columns: clangd's
error, warning, info or hint icon, a shield for the scanners and a robot for the local model, each
colored by its worst finding (set `vim.g.rick_margin_letters = true` in init.lua for the old
`E`/`W`/`I`/`H`, `S` and `M` letters if your font lacks the icons). Rest the mouse on one to read that
column's messages (nothing pops up over the code itself, by mouse or cursor; `Space cd` shows
everything on the cursor line). The popup shows one finding at a time, worst first, as `◀ 1/3 ▶`:
click an arrow, scroll the wheel over it, or press Left / Right once you're in it (`Space cd`
puts you there). Its Copy button copies the message, and Copy fix copies the local model's
suggested fix. Click into the popup to select text: dragging copies
it, as does `y` on a selection or `yy`; `Esc` or `q` goes back to the file. A call that's missing arguments shows their names in red inside
the parentheses or braces once you stop typing (the popup shows their types), and parameter names appear before the arguments
you did pass (`Space th` toggles those). When there are several constructors, the popup shows
one at a time (`◀ 1/3 ▶`): scroll the wheel over it, click an arrow, or press `]o` / `[o`
(Cmd+P opens it); the red hint follows. That's `lua/config/diagnostics.lua`; the hint kinds
are set in `Syllable/.clangd`.

cppcheck and semgrep scan each C/C++ file in the background when it opens and every time it's
saved. What they find is the `S` next to the `E`, red for errors and yellow for warnings;
resting the mouse on the `S` shows the message, which tool found it and its id (select and
copy it the same way). Silence one finding with
`// cppcheck-suppress <id>` or `// nosemgrep: <id>` on the line above it. A project can use the local model's `nomodel` style instead by setting `vim.g.scanner_nomodel = true` in its `.nvim.lua`: `// nocppcheck` and `// nosemgrep` (end of the line or the line above, `: id` for one finding, `-begin`/`-end` around several lines); a note after `--` (`// nocppcheck: nullPointer -- checked by the caller`) keeps the finding as a green shield showing the note. semgrep uses the
project's `.semgrep.yml` if there is one, otherwise `semgrep/c-cpp.yml` here (unsafe string
functions, non-literal format strings, use after free, double free, `delete` on `new[]`).
`Space tc` / `Space ts` turn each one off. That's `lua/config/scanners.lua`.

A model running on the Mac also reads each C/C++ function in the background, when the file
opens and a couple of seconds after it's saved. What it finds is the `M` next to the `S`
(the margin keeps three columns: `E` clangd, `S` cppcheck/semgrep, `M` the model); rest the
mouse on the `M` for the message. Only functions that changed are sent again, and it never
holds up typing. It needs Ollama and a small coding model, once:

```sh
.nvim/install.sh --local-model      # or by hand: brew install ollama, brew services start ollama,
                                    # ollama pull qwen2.5-coder:7b (about 4.7 GB)
```

The checks are prompt files in `mcp/checks/`: `c-bugs.md` (general C/C++ bugs) and
`c-stl-master.md` (standard library misuse such as iterator invalidation, unchecked `find`,
`remove` without `erase`, `map[]` used as a lookup; based on the C++ STL Master skill and
kept to C++98 fixes). Add a `.md` file there to add a check; a `languages: cpp` line under
its title keeps it to those filetypes, otherwise it runs on C and C++. `Space tm` turns the checks on and
off (remembered), `Space cm` runs them now. Another model or endpoint goes in `init.lua`
before the setup: `vim.g.rick_mcp_model = "qwen2.5-coder:14b"`, or
`vim.g.rick_mcp_url = "http://localhost:1234/v1"` for LM Studio. `mcp/server.py` is a plain
MCP server, so Claude Code can use the same checks:
`claude mcp add local-model -- python3 ~/.config/nvim/mcp/server.py`. That's
`lua/config/mcp.lua`.
The full guide (models, writing checks, troubleshooting) is `../local-ai.md`.

When GCC 15 lands, change `gnu++98` to `gnu++20` in `.clangd`.

## TODO

- Make the CLion keys terminal-agnostic. Today Cmd keys and Shift Shift depend on the iTerm2
  profile and Karabiner, which are macOS only. Ideas: detect the terminal (`$TERM_PROGRAM`,
  `$KITTY_WINDOW_ID`, `$WEZTERM_EXECUTABLE`) and ship key configs for kitty, WezTerm,
  Ghostty, Alacritty and GNOME Terminal / Konsole (Super instead of Cmd on Linux); fall back
  to Ctrl/Alt equivalents when the terminal can't send Cmd/Super; give Shift Shift a
  Linux path (keyd or interception-tools, or kitty's key reporting for Shift only); have
  `:checkhealth` or a startup check say which keys this terminal can't send.
