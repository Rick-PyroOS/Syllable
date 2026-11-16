# Local AI for Syllable

## Summary

A small coding model running on the Mac reviews the C and C++ code in this repo while you
work. Every check is a plain prompt file. The model runs in Ollama, and a small MCP server
in `.nvim/mcp/` connects it to Neovim and to anything else that speaks MCP, such as Claude
Code. In Neovim, its findings show up as an `M` in the margin next to `E` (clangd) and `S`
(cppcheck/semgrep).

Nothing in this setup sends code off the machine. The model's findings are suggestions,
not proof: treat an `M` the way you would a comment from a reviewer who is quick but
sometimes wrong.

## What runs where

| Piece | What it is | Where it runs |
|-------|------------|---------------|
| Ollama | runs the model, answers on `http://localhost:11434` | on the Mac (Apple GPU through Metal) |
| `qwen2.5-coder:7b` | the default model, about 4.7 GB on disk | loaded by Ollama |
| `.nvim/mcp/server.py` | MCP server that turns check prompts into model requests | started by Neovim as a child process |
| `.nvim/lua/config/mcp.lua` | splits files into functions, sends them, draws the `M` | inside Neovim |
| `.nvim/mcp/checks/*.md` | the checks, one prompt per file | read by the server |

The server uses only the Python standard library. Its only network call is to the model
endpoint, which defaults to localhost, and it skips any HTTP proxy for localhost. Code
could leave the Mac in only two ways, and both would be your own choice: pointing
`vim.g.rick_mcp_url` at another machine, or picking one of Ollama's cloud models (names
ending in `-cloud`).

## Setup (once)

From the Syllable checkout:

```sh
.nvim/install.sh --local-model                        # model picked by memory size
.nvim/install.sh --model qwen2.5-coder:7b             # or name the model yourself
```

- **macOS:** installs Ollama with Homebrew and starts it as a login service
  (`brew services`).
- **Linux:** runs Ollama's official installer, which adds a systemd service and the GPU
  runtime and asks for sudo. Without systemd (a container, or WSL without it), the
  script starts `ollama serve` in the background and reminds you to start it yourself
  next time.
- **Both:** downloads the model (3b under 16 GB of memory, 7b under 32 GB, 14b
  otherwise; see the table below) and saves its name where Neovim reads it, so nothing
  needs editing.

The model is off by default because it's a download of several GB. Running the script
again skips whatever is already in place, and `--dry-run` shows what it would do.

By hand instead:

```sh
brew install ollama                                   # Linux: curl -fsSL https://ollama.com/install.sh | sh
brew services start ollama                            # or run  ollama serve  in a terminal
ollama pull qwen2.5-coder:7b                          # about 4.7 GB, once
```

Check that it works:

```sh
ollama list                       # the model should be listed
ollama run qwen2.5-coder:7b "What does std::vector::erase return?"
```

Then open any `.c` or `.cpp` file in Neovim. Until Ollama answers, you get one warning per
session saying what's missing.

## Picking a model

Bigger models find more real problems and invent fewer, but take longer per function.

| Mac memory | Model | Size | Notes |
|------------|-------|------|-------|
| 8 GB | `qwen2.5-coder:3b` | 1.9 GB | fast, misses more |
| 16 GB | `qwen2.5-coder:7b` | 4.7 GB | the default |
| 32 GB | `qwen2.5-coder:14b` | 9 GB | noticeably better reviews; a good fit for the M2 with 32 GB |
| 64 GB+ | `qwen2.5-coder:32b` | 20 GB | best results, slow on long files |

To switch, run the installer again with the model you want:
`.nvim/install.sh --model qwen2.5-coder:14b`. It pulls the model and Neovim uses it the
next time it starts. To pin a model in the config instead, set it in `.nvim/init.lua`,
above the line `require("config.mcp").setup()`. This wins over what the installer saved:

```lua
vim.g.rick_mcp_model = "qwen2.5-coder:14b"
```

To compare models on real code before choosing, pull the ones you want to try and run
`python3 .nvim/mcp/bench.py` (or give it a file of your own). It times every installed
coder model with the same checks Neovim runs and lists what each one reports, so you can
weigh speed against false alarms.

Any OpenAI-compatible server on the Mac works too. LM Studio, for example:
`vim.g.rick_mcp_url = "http://localhost:1234/v1"`.

## Using it in Neovim

- **When it runs:** when a C or C++ file opens, and two seconds after each save (so
  auto-save doesn't start a run on every keystroke). It runs in the background and never
  holds up typing.
- **What gets sent:** each function, class or other top-level definition on its own,
  found with treesitter. It looks inside namespaces, `extern "C"` blocks and `#if` blocks,
  skips anything under three lines, and splits anything over 300 lines into pieces.
- **What gets re-sent:** after a save, only functions whose text changed go back to the
  model. Moving a function without changing it doesn't count. Editing a check's prompt or
  switching models re-runs that check on everything.
- **Reading results:** an `M` in the third margin column means the model flagged that
  line. Red is an error, yellow a warning, blue a note. Rest the mouse on the `M` to read
  the message, which check raised it and the model's suggested fix, or press `Space cd`
  on the line. The fix is only a suggestion: nothing changes in the file until you edit
  it yourself, and a small model's fix can be wrong.
- **Keys:** `Space tm` turns the checks on and off (remembered across restarts), and
  `Space cm` runs them on the current file right away.

The model handles one request at a time, so a big file fills in function by function.
Ollama unloads an idle model after five minutes, which makes the first function after a
break a few seconds slower. Run `launchctl setenv OLLAMA_KEEP_ALIVE 30m` (then restart
Ollama) to keep it loaded longer.

## The checks

Every check runs on every function it applies to. Each one is a separate pass of the
model, so each extra check adds to how long a file takes.

| Check | Files | Looks for |
|-------|-------|-----------|
| `c-bugs.md` | C and C++ | leaks on early returns, use after free, double free, mismatched new/delete, buffer overruns, off-by-one loops, NULL dereferences, ignored error returns, locks not released, missing virtual destructors |
| `c-stl-master.md` | C++ only | standard library misuse: iterators used after `erase` or `push_back`, `find` used without checking `end()`, `remove` without `erase`, `map[]` used as a lookup, `binary_search` on unsorted data, comparators using `<=`, container passed by value |
| `errno.md` | C and C++, only functions that set errno | `errno = E…` codes whose meaning doesn't fit what failed, for example `ENAVAIL` (No XENIX semaphores available) after a save fails. The server adds what each code means, read from Syllable's `errno.h` |

`c-stl-master.md` is based on the topics of the
[C++ STL Master](https://mcpmarket.com/tools/skills/c-stl-master) skill. That skill is a
tutorial that pushes C++17/20 features. This repo builds as `gnu++98`, so the check only
suggests fixes that compile in C++98. Once GCC 15 lands and `.clangd` moves to `gnu++20`,
take that paragraph out of the check.

### Writing a check

Add a Markdown file to `.nvim/mcp/checks/`. The file name (without `.md`) is the check's
name, and the first line is its title:

```markdown
# Syllable kernel locking check
languages: c cpp

You review code from the Syllable kernel. Look only for:
- a spinlock held across a call that can sleep
- lock_semaphore() without unlock_semaphore() on every return path
...

Don't report style or naming.

Lines show their number before the "|". Point at the line where the mistake is.
```

- The optional `languages:` line limits the check to those Neovim filetypes (`c`,
  `cpp`). Without it, the check runs on both C and C++.
- The optional `when:` line is a regular expression: the check only runs on functions
  that match it, so it costs nothing on the rest (`errno.md` uses `when: \berrno\s*=[^=]`).
- `context: errno` adds what each errno code in the function means, plus a list of
  common codes, to what the model reads.
- You don't need to describe the answer format. The server adds the JSON instructions
  itself and drops anything that doesn't point at a real line in the function.
- Saving the file is enough. The next run (`Space cm`, or the next save of a C/C++ file)
  uses the new prompt.

Small models do best with a short list of concrete mistakes, a line about what not to
report, and examples written in the code's own style. Asking for "anything wrong" gets
vague or invented findings. If a check is noisy, add to its "don't report" line rather
than deleting the check.

To silence one finding you've checked, add a comment the way you would for semgrep:
`// nomodel` at the end of the line silences every check there, and `// nomodel: c-bugs`
silences only that check. Either one alone on the line above works for the next line.
Add a note after `--` to keep the finding but mark it checked instead:
`// nomodel: c-bugs -- Settings deletes the File`. The robot in the margin turns green and
the popup shows the note at the bottom.
For several lines, put `// nomodel-begin` (with the same check names and note) before them
and `// nomodel-end` after. A begin with no end runs to the end of the file.
It takes effect when the file is next checked (after a save, or `Space cm`).

## From Claude Code

The same server works as an MCP server for Claude Code. Run this once, from the Syllable
folder:

```sh
claude mcp add local-model -- python3 ~/.config/nvim/mcp/server.py
```

Add `-s user` to make it available in every project. Claude then has three tools:

| Tool | What it does |
|------|--------------|
| `list_checks` | lists the checks, the model and the endpoint |
| `run_check` | runs one check on a file (`path`) or on a piece of code (`code`, with `start_line` for real line numbers) and returns the findings as JSON |
| `ask` | sends any question to the local model and returns its answer |

For example: "use local-model to run c-stl-master on
system/apps/Assistant/settings/panels/panel.cpp". This is useful for cheap first passes
over many files.

Through Claude Code, the code still reaches Claude itself, which runs in the cloud. The
"nothing leaves the Mac" guarantee applies only to the Neovim path.

## Settings

| Setting | Where | Default |
|---------|-------|---------|
| model | `vim.g.rick_mcp_model`, else the one `install.sh` saved in `~/.local/state/nvim/mcp.json`, else `RICK_MCP_MODEL` in the environment | `qwen2.5-coder:7b` |
| endpoint | `vim.g.rick_mcp_url`, or `RICK_MCP_URL` | `http://localhost:11434/v1` |
| checks folder | `RICK_MCP_CHECKS` | `.nvim/mcp/checks` |
| request timeout (seconds) | `RICK_MCP_TIMEOUT` | `300` |
| on or off | `Space tm` (saved in `~/.local/state/nvim/mcp.json`) | on |

## Troubleshooting

| Message | Fix |
|---------|-----|
| `can't reach the local model at http://localhost:11434/v1` | Ollama isn't installed or running: `.nvim/install.sh --local-model`, or `brew services start ollama` (Linux: `sudo systemctl start ollama`, or `ollama serve`) |
| `the model qwen2.5-coder:7b isn't installed` | no model is downloaded yet: `.nvim/install.sh --local-model`, or `ollama pull qwen2.5-coder:7b`. If a different model is downloaded, it's used instead, with a one-time note saying which |
| `python3 isn't installed` | `xcode-select --install`, or `brew install python` |
| no `M` at all and no warning | `Space tm` may have turned it off; `:lua print(require("config.mcp").enabled)` says |
| an `M` that's clearly wrong | the model is guessing. Try a bigger model, or tighten the check's prompt |

To try the server without Neovim:

```sh
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"list_checks","arguments":{}}}' \
  | python3 ~/.config/nvim/mcp/server.py
```
