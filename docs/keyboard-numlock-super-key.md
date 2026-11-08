# NumLock at startup and Super key support in Syllable

Read-only review of `~/Code/Rick-PyroOS/Syllable` on branch `feat/input-server`. All paths are relative to the repository root.

Path shorthands used below:
- `appserver/` = `system/sys/appserver/appserver/`
- `inputserver/` = `system/sys/servers/inputserver/`

## 1. NumLock on at startup

**Today:** only half works, and no shipped keymap turns it on.

How it is meant to work: a keymap source file can have `InitialLocks = NumLock`. `keymapconv` turns that into the binary keymap's `m_nLockSetting` field (`KLOCK_NUMLOCK = 0x04`, `system/sys/include/syllable/appserver/keymap.h:42-46`, field at `:75`).

What actually happens:
- The keyboard driver constructor (`appserver/drivers/input/keyboard/keyboard.cpp:70-103`) calls `IOCTL_KBD_LEDRST` (`:75`), then `switch ( g_psKeymap->m_nLockSetting )` (`:76`), and turns the NumLock **LED** on.
- The appserver's own lock state `static uint32 g_nLock = 0;` (`appserver/server/keyboard.cpp:50`) is **never** set from it. It only changes when a lock key is pressed (`SetQualifiers()`, `:135-151`), and `convert_key_code()` reads it for the keypad (`:240-242`). So the LED would be on while the keypad still acts like NumLock is off. Your first NumLock press would then turn the LED off and the numbers on.
- The driver uses a `switch` on one value, so a combination such as Caps+Num (0x05) falls through to "Unknown lock key" (`appserver/drivers/input/keyboard/keyboard.cpp:99-101`).
- The kernel side only toggles. `system/sys/kernel/drivers/ps2/ps2.c:363-376` handles `IOCTL_KBD_LEDRST` and does `g_nKbdLedStatus ^= ...` for scroll/num/caps, then sends `PS2_CMD_KBD_SETLEDS` (0xED, `ps2.h:68`) at `ps2.c:388`. `ps2_keyboard_init()` (`ps2.c:476-479`) starts with every LED off.
- `keymapconv` parses `InitialLocks` with `%127[a-zA-Z]`, so `InitialLocks = CapsLock NumLock` keeps only the first word (`system/apps/utils/keymapconv/keymapconv.cpp:205-222`).
- All 173 `.map` files in `system/other/keymaps/` leave it off. 10 of them have a bare `InitialLocks =` line, which is what keymapconv writes when decompiling (`keymapconv.cpp:316`). The template has it commented out (`system/other/keymaps/Syllable-keymap.template:44-45`).
- Order at startup is fine: the config and keymap are loaded (`appserver/server/server.cpp:1177`, `InitKeyboard()` at `:1186`) before `AppServer::Run()` creates the input drivers (`InitInputSystem()`, `:864`).
- A runtime keymap change (`AppserverConfig::SetKeymapPath()`, `appserver/server/config.cpp:173`) doesn't re-apply locks or LEDs.

**Fix needed (small):**
1. In `appserver/server/keyboard.cpp`, set `g_nLock = g_psKeymap->m_nLockSetting` in `SetKeymap()` (`:360`) / `InitKeyboard()` (`:401`).
2. In the driver constructor, test each bit instead of a `switch`.
3. Make keymapconv accept several words for `InitialLocks`.
4. Add `InitialLocks = NumLock` to your keymap source (for example `system/other/keymaps/American.map`) and rebuild keymaps (`system/other/keymaps/Makefile:19`, `CMakeLists.txt`).

After 1-2, step 4 alone is how you'd enable it per keymap.

## 2. Super / Windows key

**Today:** not supported. The keys are dropped in the kernel and never reach the appserver.

- PS/2: the extended scancode table maps E0 5B / 5C / 5D (Left Win, Right Win, Menu) to `0x00` (`system/sys/kernel/drivers/ps2/ps2.h:269-271`). `ps2.c:241-258` looks the code up in that table and discards any key that maps to 0 (`if ( (nKey | nFlg) == nLastKey || 0 == nKey ) return( 0 );`, `:253`).
- USB HID: same problem. `system/sys/kernel/drivers/input/usb_hid/hid.h:209` maps the GUI keys to 125/126 (and Menu to 127), but `hid.h:342-344` maps 125/126/127 to 0, and `hid.c:430` (`if( bKeyCmd && ( nKeyVal & ~0x80 ) != 0 )`) drops them.
- Raw keys 0x66/0x67/0x68 are reserved for these keys ("left windows option = 0x66 / right windows option = 0x67 / menu key = 0x68", `Syllable-keymap.template:26-28`; default keymap `appserver/server/defaultkeymap.cpp:10-16`) but are never produced.
- There is no qualifier flag for Super. `system/sys/include/syllable/gui/guidefines.h:152-168` has only SHIFT/CTRL/ALT (L/R), REPEAT and DEADKEY. Bit 0x80 is free.
- The keymap's `m_nLCommand`/`m_nRCommand`/`m_nMenu` fields (`keymap.h:68-74`) are never read. `SetQualifiers()` (`appserver/server/keyboard.cpp:89-153`) hardcodes raw codes, and treats 0x5d (Left Alt, `ps2.h:141`) as `QUAL_LALT` (`:121-126`). The default keymap calls that same key `LCommand` (`defaultkeymap.cpp:10`).
- Window management uses **Left Alt** as the "Command" key (`appserver/server/server.cpp`): Alt+W close (`:329`), Alt+Q quit (`:343`), Alt+F1-F12 desktops and Alt+Tab switcher (`:412-436`). Ctrl+Alt is for system actions: Del reboots (`:313-318`) and Backspace reloads the config (`:358-377`). Ctrl+click raises a window (`appserver/server/swindow.cpp:1275`).
- `ShortcutKey` (`system/sys/appserver/libsyllable/util/shortcutkey.cpp:13-21`) only knows `ALT+`, `SHIFT+` and `CTRL+`.

**Fix needed (medium):**
1. Kernel: map E0 5B/5C/5D to 0x66/0x67/0x68 in `system/sys/kernel/drivers/ps2/ps2.h` and in the USB table in `system/sys/kernel/drivers/input/usb_hid/hid.h`.
2. Add `QUAL_LSUPER`/`QUAL_RSUPER`/`QUAL_SUPER` in `guidefines.h` (using 0x80 and a free higher bit).
3. Handle 0x66/0x67 in `SetQualifiers()`. Better still, read the keymap's `m_n*` modifier fields instead of hardcoding them.
4. Add a `SUPER+` token to `ShortcutKey`.
5. Optional: decide what Super does in the window manager, for example the Win key opening the launcher or Super+F-keys switching desktops, or making Super the window-management key instead of Left Alt.

Step 1 alone makes the keys produce raw codes that apps can see as plain key events. Steps 2-4 make Super a real modifier.

## 3. The inputserver on this branch

`feat/input-server` adds `inputserver/` (Dee Sharpe's 2009 code). Its keyboard code is a copy of the appserver's, so it has the same gaps:

- `inputserver/server/keyboard.cpp`: `static uint32 g_nLock = 0;` (`:48`) is never set from the keymap. `SetQualifiers()` (`:87`) has the same hardcoded raw codes, with 0x5d as `QUAL_LALT` (`:119`). `convert_key_code()` uses `g_nLock` for the keypad (`:238`). `SetKeymap()` is at `:358` and `InitKeyboard()` at `:399`.
- `inputserver/server/defaultkeymap.cpp:10-17`: the same values as the appserver default keymap (LCommand 0x5d, Menu 0x68, LockSetting 0x00).
- `system/sys/include/servers/inputserver/keymap.h`: the same struct and `KLOCK_*` values as the appserver header (`KLOCK_NUMLOCK` at `:48`, `m_nLCommand` at `:71`, `m_nLockSetting` at `:78`). It differs only in comments and whitespace.
- Keyboard device lock switch: `inputserver/devices/keyboard/keyboard.cpp:219-244`, in `KeyboardDriver::InitCheck()`. It has the same single-value `switch ( g_psKeymap->m_nLockSetting )`. The older copy in `inputserver/devices/keyboard/help/keyboard.cpp:75-100` has it too. `inputserver/devices/usbkeyboard/keyboard.cpp` doesn't set lock LEDs at all.
- The inputserver still gets its keys from the same kernel drivers, so the Super/Menu keys are lost before it sees them too.

**Fixes:** sections 1 and 2 apply here as well. Make the same `g_nLock`, lock-bit and `SetQualifiers()` changes in `inputserver/server/keyboard.cpp` and `inputserver/devices/keyboard/keyboard.cpp`. If both servers are kept, it's better to share one keymap/qualifier implementation than to patch two copies.

**Note:** this code isn't wired up yet. Nothing in `inputserver/` calls `InitKeyboard()`, `inputserver/server/keyboard.cpp:403` still calls the appserver's `AppserverConfig`, and the `inputserver/devices/*/CMakeLists.txt` files are empty stubs. Fix the keyboard logic when the inputserver is ported, not as a separate step.
