# Neovim for Raycast

Drive the running Neovim from Raycast, like Search Menu Items does for Mac apps.

| Command | What |
|---|---|
| **Search Neovim Actions** | every shortcut this config sets, by name with its keys, plus every `:` command. Enter runs it in Neovim. |
| **Switch Neovim Tab** | the files open in Neovim, most recent first. Enter switches to it. |
| **Open File in Neovim** | fuzzy-find any file in Neovim's project (git's file list). Enter opens it, Shift+Enter opens it in a right split. |

After running something, Raycast closes and brings iTerm to the front so you see it.
With more than one Neovim open, a dropdown in the search bar picks which one (the one
you used last is first). Give each command a hotkey in Raycast Settings > Extensions.

## Install (once)

Needs Node 22.22 or newer (`node --version`; `brew upgrade node` if older).

```sh
cd Syllable/.raycast     # your Syllable checkout
npm install
npm run dev
```

`npm run dev` adds the extension to Raycast; it stays after you stop it with Ctrl+C.

## How it talks to Neovim

`Syllable/.nvim/lua/config/raycast.lua` makes each Neovim listen on a socket in
`~/.local/state/nvim/raycast/<pid>.sock`. The extension runs
`nvim --server <socket> --remote-expr ...` to list actions, tabs and the project folder,
and to run things. Running leaves insert mode and closes any open picker first.

Preferences (Raycast Settings > Extensions > Neovim): the `nvim` binary
(default `/opt/homebrew/bin/nvim`) and the terminal to bring forward (default iTerm).

If Neovim is showing a "Press ENTER" prompt it can't answer until you dismiss it;
Raycast then shows "Neovim didn't answer".
