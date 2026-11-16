#!/usr/bin/env bash
# Sets up Neovim for Syllable on macOS or Linux, from this folder (.nvim in the
# Syllable checkout):
#
#   1. installs what the config needs: Neovim 0.11+, git, ripgrep, fd, the
#      tree-sitter CLI, a C compiler, Python 3, doxygen, jq, cppcheck and semgrep,
#      chafa (macOS: image previews), and the JetBrains Mono Nerd Font (macOS: Homebrew; Linux: apt, dnf, pacman
#      or zypper, plus downloads into ~/.local for what the distro lacks or has too
#      old; on Linux also xdg-utils for opening the browser and xclip /
#      wl-clipboard for the clipboard, which macOS has built in)
#   2. links ~/.config/nvim to this folder (an existing config is moved aside to
#      ~/.config/nvim.backup-<date>, never deleted)
#   3. installs the plugins (at the versions in lazy-lock.json), the tree-sitter
#      parsers and the Mason tools (clangd, clang-format, ...)
#   4. macOS only, for the apps that are installed: the iTerm2 profile that passes
#      Cmd keys to Neovim, the Karabiner Shift Shift rule, the Hammerspoon Shift
#      Shift script, the Ghostty config include, and the Raycast extension's
#      packages (../.raycast)
#   5. with --local-model: Ollama and a coding model for the local model checks
#      (the M in the margin; see ../local-ai.md). Off by default: it's a large
#      download.
#
# Safe to run again: anything already in place is left alone.

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: .nvim/install.sh [options]

  --dry-run             print what would be done, change nothing
  --appname NAME        install as ~/.config/NAME and run it with NVIM_APPNAME=NAME,
                        leaving an existing ~/.config/nvim alone (default: nvim;
                        the Raycast extension only finds the default)
  --iterm-parent NAME   iTerm2 profile the "Neovim (CLion keys)" profile inherits
                        colors and font from (default: Default)
  --no-font             skip the Nerd Font
  --no-scanners         skip cppcheck and semgrep
  --no-plugins          skip installing plugins, parsers and Mason tools
  --no-mac-extras       skip iTerm2, Karabiner and Raycast setup
  --local-model         also install Ollama and a coding model for the local model
                        checks (the M in the margin), picked by how much memory
                        this machine has: 3b under 16 GB, 7b under 32 GB, else 14b
  --model NAME          the Ollama model to pull instead (implies --local-model),
                        e.g. qwen2.5-coder:7b
  -h, --help            this help
EOF
}

NVIM_MIN=0.11.0
TS_MIN=0.25.0 # tree-sitter CLI that nvim-treesitter's main branch needs
NODE_MIN=22.22.0 # Raycast's tooling

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
REPO="$(dirname "$HERE")"
LOCAL="$HOME/.local"
BIN="$LOCAL/bin"
OS="$(uname -s)"
ARCH="$(uname -m)"

DRY=0 FONT=1 SCANNERS=1 PLUGINS=1 MAC_EXTRAS=1 LOCAL_MODEL=0
APPNAME="nvim" ITERM_PARENT="Default" MODEL=""
OLLAMA_URL="http://localhost:11434"
NEXT="" # steps for the person to do afterwards, one per line

while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1 ;;
    --appname) APPNAME="${2:?--appname needs a name}"; shift ;;
    --iterm-parent) ITERM_PARENT="${2:?--iterm-parent needs a profile name}"; shift ;;
    --no-font) FONT=0 ;;
    --no-scanners) SCANNERS=0 ;;
    --no-plugins) PLUGINS=0 ;;
    --no-mac-extras) MAC_EXTRAS=0 ;;
    --local-model) LOCAL_MODEL=1 ;;
    --model) MODEL="${2:?--model needs a name, e.g. qwen2.5-coder:7b}"; LOCAL_MODEL=1; shift ;;
    -h | --help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

say() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }
die() { printf '\033[1;31merror:\033[0m %s\n' "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }
next() { NEXT="$NEXT$*"$'\n'; }

# Run a command, or only show it with --dry-run.
run() {
  if [ "$DRY" = 1 ]; then printf '    $ %s\n' "$*"; else "$@"; fi
}

# version_ge A B: is version A >= B? (dotted numbers; anything after them is ignored)
version_ge() {
  awk -v a="$1" -v b="$2" 'BEGIN {
    split(a, x, "."); split(b, y, ".")
    for (i = 1; i <= 3; i++) { if (x[i] + 0 > y[i] + 0) exit 0; if (x[i] + 0 < y[i] + 0) exit 1 }
    exit 0 }'
}

nvim_version() { nvim --version 2>/dev/null | head -1 | sed -e 's/^NVIM v//' -e 's/[^0-9.].*//'; }
ts_version() { tree-sitter --version 2>/dev/null | awk '{ print $2 }'; }
node_version() { node --version 2>/dev/null | sed 's/^v//'; }

# download URL FILE
download() { run curl -fL --retry 3 --progress-bar -o "$2" "$1"; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ---------------------------------------------------------------------------
# macOS

mac_packages() {
  if ! have brew; then
    for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
      if [ -x "$b" ]; then eval "$("$b" shellenv)"; break; fi
    done
  fi
  have brew || die "Homebrew is needed on macOS: install it from https://brew.sh, then run this again."

  if ! xcode-select -p >/dev/null 2>&1; then
    run xcode-select --install || true
    die "Finish installing the Xcode Command Line Tools (the C compiler), then run this again."
  fi

  local want="neovim git ripgrep fd tree-sitter-cli doxygen jq chafa" missing="" p
  [ "$SCANNERS" = 1 ] && want="$want cppcheck semgrep"
  for p in $want; do brew list --formula "$p" >/dev/null 2>&1 || missing="$missing $p"; done
  if [ -n "$missing" ]; then
    say "Installing with Homebrew:$missing"
    # shellcheck disable=SC2086
    run brew install $missing
  fi
  if have nvim && ! version_ge "$(nvim_version)" "$NVIM_MIN"; then
    say "Upgrading Neovim (have $(nvim_version), need $NVIM_MIN+)"
    run brew upgrade neovim
  fi
  if have tree-sitter && ! version_ge "$(ts_version)" "$TS_MIN"; then
    say "Upgrading the tree-sitter CLI"
    run brew upgrade tree-sitter-cli
  fi
  if [ "$FONT" = 1 ] && ! brew list --cask font-jetbrains-mono-nerd-font >/dev/null 2>&1; then
    say "Installing the JetBrains Mono Nerd Font"
    run brew install --cask font-jetbrains-mono-nerd-font
  fi
  [ "$FONT" = 1 ] && next "Pick \"JetBrainsMono Nerd Font\" as your terminal's font (iTerm2: Settings > Profiles > Text)."
  return 0
}

app_installed() { [ -d "/Applications/$1.app" ] || [ -d "$HOME/Applications/$1.app" ]; }

mac_extras() {
  if app_installed iTerm; then
    say "Writing the iTerm2 profile \"Neovim (CLion keys)\" (inherits from \"$ITERM_PARENT\")"
    run python3 "$HERE/extras/iterm2/make_profile.py" --parent "$ITERM_PARENT" \
      --out "$HOME/Library/Application Support/iTerm2/DynamicProfiles/neovim-clion-keys.json"
    next "iTerm2: Settings > Profiles > \"Neovim (CLion keys)\" > Other Actions > Set as Default (Cmd shortcuts reach Neovim only in that profile)."
  else
    say "iTerm2 isn't installed; skipping its profile (Cmd shortcuts need it)."
  fi

  if app_installed Karabiner-Elements; then
    local dir="$HOME/.config/karabiner/assets/complex_modifications"
    say "Adding the Karabiner rule for Shift Shift"
    run mkdir -p "$dir"
    run cp "$HERE/extras/karabiner/neovim-shift-shift.json" "$dir/"
    next "Karabiner-Elements: Complex Modifications > Add predefined rule > \"Neovim: Shift Shift opens Find\" > Enable."
  else
    next "Shift Shift (Find) needs Karabiner-Elements: brew install --cask karabiner-elements, open it once to grant its permissions, then run this again."
  fi

  if app_installed Hammerspoon; then
    local hs="$HOME/.hammerspoon/init.lua"
    local hs_line="dofile(os.getenv(\"HOME\") .. \"/.config/nvim/extras/hammerspoon/shift_shift.lua\")"
    if [ -f "$hs" ] && grep -qF "extras/hammerspoon/shift_shift.lua" "$hs"; then
      say "Hammerspoon already loads the Shift Shift script"
    else
      say "Loading the Shift Shift script from $hs"
      run mkdir -p "$HOME/.hammerspoon"
      if [ "$DRY" = 1 ]; then
        run printf '%s\n' "$hs_line" ">>" "$hs"
      else
        printf '%s\n' "$hs_line" >>"$hs"
      fi
      next "Hammerspoon: Reload Config from its menu bar icon (and grant it Accessibility access if asked)."
    fi
  fi

  if app_installed Ghostty; then
    local gh="$HOME/.config/ghostty/config"
    local gh_line="config-file = ~/.config/nvim/extras/ghostty/config"
    if [ -f "$gh" ] && grep -qxF "$gh_line" "$gh"; then
      say "Ghostty already includes the Neovim config"
    else
      say "Including the Neovim Ghostty config from $gh"
      run mkdir -p "$(dirname "$gh")"
      if [ "$DRY" = 1 ]; then
        run printf '%s\n' "$gh_line" ">>" "$gh"
      else
        printf '%s\n' "$gh_line" >>"$gh"
      fi
      next "Ghostty: reload its config (Cmd+Shift+,) or restart it."
    fi
  fi

  if app_installed Raycast && [ -f "$REPO/.raycast/package.json" ]; then
    if ! have node || ! version_ge "$(node_version)" "$NODE_MIN"; then
      say "Installing Node.js for the Raycast extension"
      if brew list --formula node >/dev/null 2>&1; then run brew upgrade node; else run brew install node; fi
    fi
    say "Installing the Raycast extension's packages"
    if [ "$DRY" = 1 ]; then
      run npm --prefix "$REPO/.raycast" install --no-audit --no-fund
    else
      (cd "$REPO/.raycast" && npm install --no-audit --no-fund)
    fi
    next "Raycast: run  cd \"$REPO/.raycast\" && npm run dev  once; stop it with Ctrl+C after Raycast shows the Neovim commands."
  fi
}

# ---------------------------------------------------------------------------
# Linux

SUDO=""

linux_packages() {
  if [ "$(id -u)" -ne 0 ]; then
    have sudo || die "Installing packages needs sudo (or run this as root)."
    SUDO="sudo"
  fi
  local scan=""
  [ "$SCANNERS" = 1 ] && scan="cppcheck"

  if have apt-get; then
    say "Installing packages with apt"
    run $SUDO apt-get update
    # shellcheck disable=SC2086
    run $SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y \
      git curl ca-certificates unzip tar gzip xz-utils build-essential \
      ripgrep fd-find python3 python3-venv fontconfig doxygen jq xdg-utils xclip wl-clipboard $scan
  elif have dnf; then
    say "Installing packages with dnf"
    # shellcheck disable=SC2086
    run $SUDO dnf install -y git curl unzip tar gzip xz gcc gcc-c++ make \
      ripgrep fd-find python3 fontconfig doxygen jq xdg-utils xclip wl-clipboard $scan
  elif have pacman; then
    say "Installing packages with pacman"
    # shellcheck disable=SC2086
    run $SUDO pacman -S --needed --noconfirm git curl unzip tar gzip xz base-devel \
      ripgrep fd python fontconfig doxygen jq xdg-utils xclip wl-clipboard $scan
  elif have zypper; then
    say "Installing packages with zypper"
    # shellcheck disable=SC2086
    run $SUDO zypper --non-interactive install git curl unzip tar gzip xz gcc gcc-c++ make \
      ripgrep fd python3 fontconfig doxygen jq xdg-utils xclip wl-clipboard $scan
  else
    die "No supported package manager (apt, dnf, pacman, zypper). Install git, curl, a C compiler, ripgrep, fd, python3, doxygen and jq, then run this again."
  fi
  run mkdir -p "$BIN"

  # Debian and Ubuntu call fd "fdfind".
  if ! have fd && have fdfind; then run ln -sf "$(command -v fdfind)" "$BIN/fd"; fi

  case "$ARCH" in
    x86_64 | amd64) NV_ARCH=x86_64 TS_ARCH=x64 ;;
    aarch64 | arm64) NV_ARCH=arm64 TS_ARCH=arm64 ;;
    *) NV_ARCH="" TS_ARCH="" ;;
  esac

  # Distros often ship an older Neovim; use the official release then.
  if ! have nvim || ! version_ge "$(nvim_version)" "$NVIM_MIN"; then
    [ -n "$NV_ARCH" ] || die "No Neovim release for $ARCH; install Neovim $NVIM_MIN+ yourself, then run this again."
    say "Installing the latest Neovim release in ~/.local/opt/nvim"
    download "https://github.com/neovim/neovim/releases/latest/download/nvim-linux-$NV_ARCH.tar.gz" "$TMP/nvim.tar.gz"
    run mkdir -p "$LOCAL/opt"
    run rm -rf "$LOCAL/opt/nvim"
    run tar -xzf "$TMP/nvim.tar.gz" -C "$TMP"
    run mv "$TMP/nvim-linux-$NV_ARCH" "$LOCAL/opt/nvim"
    run ln -sf "$LOCAL/opt/nvim/bin/nvim" "$BIN/nvim"
  fi

  if ! have tree-sitter || ! version_ge "$(ts_version)" "$TS_MIN"; then
    [ -n "$TS_ARCH" ] || die "No tree-sitter CLI release for $ARCH; install tree-sitter $TS_MIN+ yourself."
    say "Installing the tree-sitter CLI in ~/.local/bin"
    download "https://github.com/tree-sitter/tree-sitter/releases/latest/download/tree-sitter-linux-$TS_ARCH.gz" "$TMP/tree-sitter.gz"
    if [ "$DRY" = 1 ]; then
      run gunzip -c "$TMP/tree-sitter.gz" ">" "$BIN/tree-sitter"
    else
      gunzip -c "$TMP/tree-sitter.gz" >"$BIN/tree-sitter"
    fi
    run chmod +x "$BIN/tree-sitter"
  fi

  # semgrep isn't packaged by most distros: give it its own Python environment.
  if [ "$SCANNERS" = 1 ] && ! have semgrep; then
    local venv="$LOCAL/share/neovim-clion/semgrep"
    say "Installing semgrep in $venv"
    run python3 -m venv "$venv"
    run "$venv/bin/pip" install --quiet --upgrade pip semgrep
    run ln -sf "$venv/bin/semgrep" "$BIN/semgrep"
  fi

  if [ "$FONT" = 1 ]; then
    local fonts="${XDG_DATA_HOME:-$LOCAL/share}/fonts/JetBrainsMonoNerd"
    if [ ! -d "$fonts" ]; then
      say "Installing the JetBrains Mono Nerd Font in $fonts"
      download "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/JetBrainsMono.tar.xz" "$TMP/font.tar.xz"
      run mkdir -p "$fonts"
      run tar -xJf "$TMP/font.tar.xz" -C "$fonts"
      have fc-cache && run fc-cache -f "$fonts" >/dev/null
    fi
    next "Pick \"JetBrainsMono Nerd Font\" as your terminal's font (only needed on the machine you type on)."
  fi
}

# ---------------------------------------------------------------------------
# The local model (--local-model)

ollama_up() { curl -fsS --max-time 2 "$OLLAMA_URL/api/version" >/dev/null 2>&1; }

# Memory in GB, rounded down (0 if unknown).
memory_gb() {
  local bytes=0
  if [ "$OS" = Darwin ]; then
    bytes="$(sysctl -n hw.memsize 2>/dev/null || echo 0)"
  elif [ -r /proc/meminfo ]; then
    bytes="$(awk '/^MemTotal:/ { print $2 * 1024 }' /proc/meminfo)"
  fi
  awk -v b="$bytes" 'BEGIN { printf "%d", b / 1024 / 1024 / 1024 + 0.5 }'
}

pick_model() {
  local gb
  gb="$(memory_gb)"
  # A little under 16 and 32: Linux reports less than what's installed.
  if [ "$gb" -ge 30 ]; then echo qwen2.5-coder:14b
  elif [ "$gb" -ge 15 ] || [ "$gb" = 0 ]; then echo qwen2.5-coder:7b
  else echo qwen2.5-coder:3b
  fi
}

install_ollama() {
  if have ollama; then
    say "Ollama is already installed"
  elif [ "$OS" = Darwin ]; then
    say "Installing Ollama with Homebrew"
    run brew install ollama
  else
    # The official installer: puts ollama in /usr/local, adds a systemd service
    # and the GPU runtime it needs (asks for sudo itself).
    say "Installing Ollama with its official installer (https://ollama.com/install.sh)"
    download "https://ollama.com/install.sh" "$TMP/ollama-install.sh"
    run sh "$TMP/ollama-install.sh"
  fi
}

start_ollama() {
  ollama_up && return 0
  say "Starting Ollama"
  if [ "$OS" = Darwin ]; then
    run brew services start ollama # also starts it at every login
  elif have systemctl && systemctl list-unit-files ollama.service >/dev/null 2>&1; then
    run $SUDO systemctl enable --now ollama
  else
    # No systemd (a container, WSL without it): run it in the background now.
    local log="${XDG_STATE_HOME:-$LOCAL/state}/ollama.log"
    run mkdir -p "$(dirname "$log")"
    if [ "$DRY" = 1 ]; then
      run nohup ollama serve ">" "$log" "2>&1" "&"
    else
      nohup ollama serve >"$log" 2>&1 &
    fi
    next "Ollama isn't a service on this machine: start it with  ollama serve  before using Neovim (or add that to your login)."
  fi
  [ "$DRY" = 1 ] && return 0
  for _ in $(seq 1 30); do ollama_up && return 0; sleep 1; done
  warn "Ollama didn't answer on $OLLAMA_URL; start it with  ollama serve  and pull the model with  ollama pull $MODEL"
  return 1
}

# Tell the Neovim config which model to use (mcp.lua reads it; vim.g.rick_mcp_model
# in init.lua still wins).
save_model() {
  local state="${XDG_STATE_HOME:-$HOME/.local/state}/$APPNAME/mcp.json"
  say "Telling Neovim to use $MODEL ($state)"
  run mkdir -p "$(dirname "$state")"
  run python3 - "$state" "$MODEL" <<'PY'
import json, sys
path, model = sys.argv[1], sys.argv[2]
try:
    with open(path) as f:
        data = json.load(f)
    if not isinstance(data, dict):
        data = {}
except (OSError, ValueError):
    data = {}
data["model"] = model
with open(path, "w") as f:
    json.dump(data, f)
PY
}

local_model() {
  [ -n "$MODEL" ] || MODEL="$(pick_model)"
  install_ollama
  if start_ollama; then
    if [ "$DRY" = 0 ] && ollama list 2>/dev/null | awk 'NR > 1 { print $1 }' | grep -qx "$MODEL"; then
      say "The model $MODEL is already downloaded"
    else
      say "Downloading the model $MODEL (several GB; once)"
      run ollama pull "$MODEL"
    fi
  fi
  save_model
}

# ---------------------------------------------------------------------------
# Both

link_config() {
  local cfg="${XDG_CONFIG_HOME:-$HOME/.config}/$APPNAME"
  if [ -L "$cfg" ] && [ "$(cd "$cfg" 2>/dev/null && pwd -P)" = "$HERE" ]; then
    say "$cfg already points here"
    return
  fi
  if [ -e "$cfg" ] || [ -L "$cfg" ]; then
    local backup
    backup="$cfg.backup-$(date +%Y%m%d-%H%M%S)"
    say "Moving your current $cfg to $backup"
    run mv "$cfg" "$backup"
  fi
  say "Linking $cfg -> $HERE"
  run mkdir -p "$(dirname "$cfg")"
  run ln -s "$HERE" "$cfg"
}

install_plugins() {
  export NVIM_APPNAME="$APPNAME"
  say "Installing plugins (versions from lazy-lock.json)"
  run nvim --headless "+Lazy! restore" +qa
  say "Installing tree-sitter parsers"
  run nvim --headless "+lua require('nvim-treesitter').install(require('config.parsers')):wait(600000)" +qa
  say "Installing clangd and the other Mason tools"
  run nvim --headless "+MasonToolsInstallSync" +qa
}

check() {
  local ok=1 t
  echo
  say "Installed:"
  for t in nvim git rg fd tree-sitter cc python3 doxygen jq; do
    if have "$t"; then printf '    %-12s %s\n' "$t" "$("$t" --version 2>&1 | head -1)"; else printf '    %-12s MISSING\n' "$t"; ok=0; fi
  done
  if [ "$SCANNERS" = 1 ]; then
    for t in cppcheck semgrep; do
      if have "$t"; then printf '    %-12s %s\n' "$t" "$("$t" --version 2>&1 | head -1)"; else printf '    %-12s MISSING\n' "$t"; ok=0; fi
    done
  fi
  if [ "$LOCAL_MODEL" = 1 ]; then
    if have ollama; then printf '    %-12s %s\n' ollama "$(ollama --version 2>&1 | tail -1)"; else printf '    %-12s MISSING\n' ollama; ok=0; fi
    if ollama list 2>/dev/null | awk 'NR > 1 { print $1 }' | grep -qx "$MODEL"; then
      printf '    %-12s %s\n' model "$MODEL"
    else
      printf '    %-12s %s MISSING\n' model "$MODEL"; ok=0
    fi
  fi
  have nvim && ! version_ge "$(nvim_version)" "$NVIM_MIN" && { warn "Neovim $(nvim_version) is older than $NVIM_MIN"; ok=0; }
  [ "$ok" = 1 ] || warn "Something above is missing; Neovim will still start, without that feature."
}

main() {
  case "$OS" in
    Darwin | Linux) ;;
    *) die "Only macOS and Linux are supported (this is $OS)." ;;
  esac
  [ -f "$HERE/init.lua" ] || die "Run this from the Syllable checkout: .nvim/install.sh"
  [ "$DRY" = 1 ] && say "Dry run: nothing will be changed"

  case ":$PATH:" in
    *":$BIN:"*) ;;
    *)
      export PATH="$BIN:$PATH"
      [ "$OS" = Linux ] && next "Add ~/.local/bin to your PATH (e.g. in ~/.bashrc or ~/.zshrc: export PATH=\"\$HOME/.local/bin:\$PATH\")."
      ;;
  esac

  if [ "$OS" = Darwin ]; then mac_packages; else linux_packages; fi
  link_config
  [ "$PLUGINS" = 1 ] && install_plugins
  [ "$OS" = Darwin ] && [ "$MAC_EXTRAS" = 1 ] && mac_extras
  [ "$LOCAL_MODEL" = 1 ] && local_model
  [ "$DRY" = 1 ] || check

  echo
  local start="nvim"
  [ "$APPNAME" = nvim ] || start="NVIM_APPNAME=$APPNAME nvim"
  say "Done. Start Neovim from the Syllable checkout: $start"
  [ "$LOCAL_MODEL" = 1 ] || next "Optional: run this again with --local-model for the local model checks (the M in the margin)."
  have claude || next "Optional: install the Claude Code CLI (claude) for the Claude pane: https://claude.com/claude-code"
  if [ -n "$NEXT" ]; then
    echo
    say "Left for you:"
    printf '%s' "$NEXT" | while IFS= read -r line; do printf '    - %s\n' "$line"; done
  fi
}

main
