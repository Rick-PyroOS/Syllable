#!/usr/bin/env python3
"""
Writes neovim-clion-keys.json: an iTerm2 dynamic profile, "Neovim (CLion keys)",
that inherits everything from your Default profile and adds key mappings so
CLion's Cmd shortcuts reach Neovim instead of iTerm's menus.

Each mapping sends a CSI u escape code (ESC [ <key> ; <modifiers> u), which
Neovim reads as <D-...> (D = Cmd). The Neovim side is
.nvim/lua/config/clion_keys.lua; keep the two lists in sync.

Install:  python3 make_profile.py [--parent PROFILE] \\
            --out ~/Library/Application\\ Support/iTerm2/DynamicProfiles/neovim-clion-keys.json
(./install.sh does this.) --parent names the profile it inherits colors and font
from (default "Default"). iTerm picks it up immediately (Settings > Profiles); the
parent profile is untouched.
Cmd+C / Cmd+V / Cmd+T / Cmd+N / Cmd+Q and other iTerm basics are not remapped.
"""
import argparse, json, os

args = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
args.add_argument("--parent", default="Default", help="iTerm profile to inherit colors and font from")
args.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "neovim-clion-keys.json"))
args = args.parse_args()

CMD, SHIFT, ALT, CTRL = 0x100000, 0x20000, 0x80000, 0x40000
NUMPAD, FN = 0x200000, 0x800000          # macOS flags iTerm records on arrow / F keys
UP, DOWN, BS, CR = 0xF700, 0xF701, 0x7F, 0x0D
F2, F3, F7, F8, F9, F10, F12, F19 = 0xF705, 0xF706, 0xF70A, 0xF70B, 0xF70C, 0xF70D, 0xF70F, 0xF716
ESC_SEQ, HEX = 10, 11                    # iTerm "Send Escape Sequence" / "Send Hex Code"

def csi_mods(mods):                      # xterm/CSI u modifier number
    return 1 + (1 if mods & SHIFT else 0) + (2 if mods & ALT else 0) + (4 if mods & CTRL else 0) + (8 if mods & CMD else 0)

SHIFTED = {"[": "{", "]": "}", "/": "?"}
keymap = {}

def add(chars, mods, action, text):
    for c in chars:
        keymap[f"0x{c:x}-0x{mods:x}"] = {"Action": action, "Text": text}

def key(ch, mods):
    """Cmd(+Shift/+Alt) + a printable key, sent as CSI u."""
    chars = [ord(ch)]
    if mods & SHIFT:
        chars.append(ord(SHIFTED.get(ch, ch.upper())))   # iTerm may record either form
    add(chars, mods, ESC_SEQ, f"[{ord(ch)};{csi_mods(mods)}u")

def special(code, mods, text, action=ESC_SEQ, flags=(0,)):
    for f in flags:
        add([code], mods | f, action, text)

# Navigation
key("o", CMD); key("o", CMD | SHIFT); key("o", CMD | ALT)       # class / file / symbol
key("e", CMD); key("a", CMD | SHIFT)                            # recent files / find action
key("p", CMD | SHIFT)                                           # command palette
special(F19, 0, "[57382u", flags=(FN, 0))                       # F19 = Shift Shift (extras/karabiner/): Find
key("b", CMD); key("b", CMD | ALT)                              # declaration / implementation
key("[", CMD); key("]", CMD)                                    # back / forward
key("l", CMD)                                                   # go to line
special(UP, CMD | CTRL, "[1;13A", flags=(NUMPAD, NUMPAD | FN))  # switch header/source
special(F12, CMD, "[24;9~", flags=(FN, 0))                      # file structure
special(F7, ALT, "[18;3~", flags=(FN, 0))                       # find usages
special(F7, CMD | ALT, "[18;11~", flags=(FN, 0))                # show usages (popup)
key("h", CTRL | ALT)                                            # call hierarchy
key("e", CMD | SHIFT)                                           # recent locations
special(F3, CMD, "[13;9~", flags=(FN, 0))                       # show bookmarks (F3 toggles one)
# Tool windows and tabs
key("1", CMD); key("w", CMD)
key("[", CMD | SHIFT); key("]", CMD | SHIFT)                    # previous / next tab
special(F12, ALT, "[24;3~", flags=(FN, 0))                      # terminal
# Search
key("f", CMD); key("r", CMD); key("f", CMD | SHIFT)
key("r", CMD | SHIFT)                                           # replace in files
# Editing
key("s", CMD); key("z", CMD); key("z", CMD | SHIFT); key("a", CMD)
key("/", CMD); key("d", CMD); key("l", CMD | ALT); key("p", CMD)
key("/", CMD | SHIFT)                                           # keymap cheat sheet
key("g", CTRL | ALT)                                            # multi-cursor: select all occurrences
key("o", CTRL | ALT)                                            # optimize #includes
key("t", CMD | ALT)                                             # surround with
key("j", CMD)                                                   # live templates
# Build
special(F9, CMD, "[20;9~", flags=(FN, 0))                       # build
special(F9, CMD | SHIFT, "[20;10~", flags=(FN, 0))              # recompile this file
special(F2, CMD, "[12;9~", flags=(FN, 0))                       # stop the build (and run / debug)
# Run and debug (lua/config/run.lua, debug.lua)
special(F10, SHIFT, "[21;2~", flags=(FN, 0))                    # run
special(F9, SHIFT, "[20;2~", flags=(FN, 0))                     # debug
special(F8, CMD, "[19;9~", flags=(FN, 0))                       # toggle breakpoint
special(F8, CMD | SHIFT, "[19;10~", flags=(FN, 0))              # show breakpoints
special(F8, SHIFT, "[19;2~", flags=(FN, 0))                     # step out
special(F8, ALT, "[19;3~", flags=(FN, 0))                       # evaluate
key("r", CMD | ALT)                                             # resume
# Settings
key(",", CMD)                                                   # settings (instead of iTerm's)
special(BS, CMD, "[127;9u")                                     # delete line
special(UP, CMD | SHIFT, "[1;10A", flags=(NUMPAD, NUMPAD | FN)) # move statement
special(DOWN, CMD | SHIFT, "[1;10B", flags=(NUMPAD, NUMPAD | FN))
special(UP, ALT | SHIFT, "[1;4A", flags=(NUMPAD, NUMPAD | FN))  # move line
special(DOWN, ALT | SHIFT, "[1;4B", flags=(NUMPAD, NUMPAD | FN))
special(CR, ALT, "0x1b 0x0d", action=HEX)                       # Alt+Enter: intention actions
special(CR, SHIFT, "[13;2u")                                    # Shift+Enter: open in right split
add([ord("n"), 0x2DC], ALT, HEX, "0x1b 0x6e")                   # Alt+N (macOS types ˜): non-project items

profile = {
    "Profiles": [{
        "Name": "Neovim (CLion keys)",
        "Guid": "neovim-clion-keys",
        "Dynamic Profile Parent Name": args.parent,  # colors, font etc. come from this profile
        "Keyboard Map": keymap,
        # Pass clicks to Neovim (open files from the tree, click between panes).
        "Mouse Reporting": True,
        # Don't copy Neovim's screens into the scrollback, so scrolling up after
        # quitting shows what was there before Neovim, not old frames of it.
        "Scrollback in Alternate Screen": False,
        # A normal macOS window (title bar, resizable), even if the parent profile is borderless.
        "Window Type": 0,
    }]
}
out = os.path.expanduser(args.out)
os.makedirs(os.path.dirname(out), exist_ok=True)
with open(out, "w") as f:
    json.dump(profile, f, indent=1, sort_keys=True)
print(f"wrote {len(keymap)} key mappings to {out}")
