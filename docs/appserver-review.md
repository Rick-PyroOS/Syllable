# Syllable appserver review

Code review of `~/Code/Rick-PyroOS/Syllable`. Behaviour below comes from reading the code unless it says it was tested. The only tests so far are the rounded window in Section 6.1 and the QEMU boots in Section 5.6. Anything marked "by reading" has not been tested.

The main text describes `main` (b16c2936). `appserver/server/` is the same on `main`, `feat/openssh` and `feat/input-server`. Section 7 lists what changes on `cairo` and `feat/input-server`.

Path shorthands:
- `server/` = `system/sys/appserver/appserver/server/`
- `decorators/` = `system/sys/appserver/appserver/drivers/decorators/`
- `registrar/` = `system/sys/appserver/registrar/`
- `libsyllable/` = `system/sys/appserver/libsyllable/`
- `include/` = `system/sys/include/syllable/`
- `kernel-drivers/` = `system/sys/kernel/drivers/`

## 0. The big picture

The appserver is one process that owns the screen, the input drivers, the window tree, font rendering and the event bus. Apps never touch the framebuffer. They send drawing commands over message ports, and the server draws for them.

Startup:
1. The kernel runs `/boot/system/programs/init` (`system/sys/kernel/kernel/init.c:565`).
2. `init` forks and execs `/system/appserver`, then becomes `/system/start.sh` (`system/apps/utils/systools/init.c:61-66`).
3. `start.sh` starts `dlogin`, then `/system/mediaserver` and `/system/registrar` (`system/image/system/start.sh:41-47`).

Inside the appserver, `main()` loads `/system/config/appserver`, calls `InitKeyboard()` and creates `AppServer` (`server/server.cpp:1138-1190`). `AppServer::Run()` creates port `gui_server_cmd`, calls `init_desktops()` and `InitInputSystem()`, and then publishes the port to the kernel with `set_app_server_port()` (`server/server.cpp:859-866`).

Threads:

| Thread | Created at | Job |
|---|---|---|
| Main loop | `server/server.cpp:851` | `DR_*` system requests, `EV_*` events, clipboard, decorator changes |
| One per app (`SrvApplication`) | `server/sapplication.cpp:1675` | `AR_*` requests: open window, fonts, bitmaps, sprites, screen modes |
| One per window (`W:<title>`) | `server/swindow.cpp:2042` | Render commands and `WR_*` window requests |
| `input_thread` | `server/input.cpp:369` | Drains the input queue, key handling, hotkeys, mouse dispatch |
| One per input driver | for example `keyboard_event_thread` | Reads the device and queues events |

Locking: `g_cLayerGate` (`server/layer.h:46-65`) is a reader/writer lock over the whole window tree. `Lock()` takes one share, for rendering. `Close()` takes all shares, for anything that changes the tree. Its count is 100000000, so readers rarely contend.

## 1. Occluded windows and window management

### 1.1 The window tree

- `Layer` (`server/layer.h:69`) is one rectangular node. An app's `os::View` maps to a `Layer`. Siblings are a linked list from `m_pcBottomChild` to `m_pcTopChild`, so list order is stacking order.
- `g_pcTopView` is a `TopLayer` (`server/toplayer.cpp`), the root, and it owns the screen.
- Each window is a `WndBorder` layer under the root (the decorator frame), with one client layer inside it (`server/swindow.cpp:86-90`).
- `SrvWindow` (`server/swindow.cpp:72`) owns those layers, the decorator, the message port and the per-desktop state.

Backdrop windows, meaning `WND_BACKMOST` such as the Desktop, always sort below normal windows (`server/layer.cpp:256-349`). `WND_SYSTEM` windows, such as menus, are tracked in a separate list per desktop.

### 1.2 How occlusion is calculated

There is no stored "is this window occluded" flag. Occlusion is the result of clipping regions. Each layer has:

| Region | Meaning |
|---|---|
| `m_pcBitmapFullReg` | Where this layer and its children may appear |
| `m_pcBitmapReg` | Where this layer itself may draw (full region minus opaque children) |
| `m_pcDamageReg` | Newly exposed or invalidated areas that still need `M_PAINT` |
| `m_pcActiveDamageReg` | The damage being repainted right now |
| `m_pcVisibleFullReg` | Top-level windows only: what is really visible on screen |

`Layer::RebuildRegion()` (`server/layer.cpp:1263`) builds these:
1. It starts from the parent's region, clipped to the layer's frame (`:1318`).
2. It intersects any shape region (`:1321-1324`).
3. It subtracts every higher, visible, non-transparent sibling (`:1329-1349`). **This loop is occlusion.** It uses the sibling's shape when it has one.
4. It subtracts visible, non-transparent children to get `m_pcBitmapReg` (`:1355-1375`), unless the layer has `WID_DRAW_ON_CHILDREN`.

Every draw call is clipped by walking that region's rectangles. `Layer::DrawBitMap` in `server/render.cpp` is a clear example. Drawing into a covered area is therefore dropped automatically.

Rebuilds are incremental. `m_bHasInvalidRegs` marks layers that need work, and `SetDirtyRegFlags()` marks them on moves and z-order changes (`server/layer.cpp:619-677`, `:944-953`).

### 1.3 The update pipeline

Every layout change ends with `UpdateRegions()` (`server/toplayer.cpp:552`, `server/layer.cpp:1406`):

1. `RebuildRegion` computes new clips, keeping the old ones in `m_pcPrev*`.
2. `MoveChilds` scrolls existing pixels for moved layers with blits, so the app doesn't repaint them.
3. `InvalidateNewAreas` computes new minus old (`server/layer.cpp:1072`) and adds it to `m_pcDamageReg`.
4. `UpdateIfNeeded` (`server/layer.cpp:512`) turns damage into an `M_PAINT` message and increments the window's paint counter (`:1788`).
5. The app draws, then replies with `WR_PAINT_FINISHED` (`server/swindow.cpp:1884`).

### 1.4 Two occlusion modes

Which one you get depends on the video driver. This matters a lot.

**Mode A: single buffer (no backbuffer).** Windows draw straight into the framebuffer. Covered parts are clipped out. When a window is uncovered, the exposed area becomes damage, the app gets `M_PAINT`, and it redraws. An app that is slow or hung leaves stale pixels until it answers.

**Mode B: per-window backbuffer.** This is Arno Klenke's work in `server/toplayer.cpp`. `TopLayer::AddChild()` calls `LayerFrameChanged()` (`:45`), which allocates an off-screen bitmap in video memory 11 pixels larger than the window, so small resizes reuse it (`:56`). The window's layers then render into that bitmap instead of the screen.
- A backbuffered window does **not** subtract its siblings (`server/layer.cpp:1315-1316`, `:1329`). It always keeps its full contents, even when completely covered.
- `TopLayer::RebuildRegion()` calculates `m_pcVisibleFullReg` separately (`server/toplayer.cpp:429-435`).
- Uncovering a window just blits from its backbuffer, with no `M_PAINT` (`server/toplayer.cpp:367-388`, `:453-497`). The app doesn't know it happened.
- Tearing is avoided: while the paint counter is above zero, the screen update waits (`server/toplayer.cpp:457`), and `WR_PAINT_FINISHED` triggers the blit (`server/swindow.cpp:1896-1902`).

**Backbuffers exist only on four drivers.** `DisplayDriver::AllocateBitmap()` needs `InitMemory()` to have been called, and only `geforcefx`, `i855`, `savage` and `sis3xx` call it. VESA, vmware, radeon, riva and the rest don't, so `AllocateMemory()` fails (`server/ddmemory.cpp:96-100`) and every window silently uses mode A. QEMU's default VGA uses VESA, so it runs in mode A (tested, Section 5.6). Enabling a system-memory fallback is possible; the code is already there behind `#if 0` (`server/toplayer.cpp:77-88`).

If video memory runs out in mode B, the window falls back to single buffer (`server/toplayer.cpp:110-114`). Apps can opt out with `WND_SINGLEBUFFER`.

Apps are never told they are occluded. Nothing tells a window to stop rendering when it is hidden behind others. There is no visibility message at all.

### 1.5 Focus, raising and input

- **Focus:** `Desktop.m_pcActiveWindow` plus a 32-entry history `m_apcFocusStack` per desktop (`server/desktop.cpp:101-112`). Closing or hiding a window pops the previous one (`server/desktop.cpp:158-201`).
- **System windows take priority:** `get_active_window()` returns the first `WND_SYSTEM` window if there is one (`server/desktop.cpp:146-156`).
- **Clicking focuses but doesn't raise.** A click raises only if "popup selected windows" is on, or on Ctrl+click or double-click on the frame (`server/swindow.cpp:1275`).
- **Depth button:** `Layer::ToggleDepth()` moves the window to the front or the back (`server/layer.cpp:586`).
- **Minimize** is a hide plus a flag (`server/wndborder.cpp:843-856`). `MakeFocus()` undoes it (`server/swindow.cpp:528-537`). `DR_MINIMIZE_ALL` is at `server/server.cpp:668`.
- **Move and resize** are driven by `WndBorder::MouseMoved()` (`server/wndborder.cpp:475`). Moves wait for the client to reply with `WR_WND_MOVE_REPLY` (`server/wndborder.cpp:1006`).
- **Alt+Tab:** `WinSelect` (`server/winselect.cpp`) shows the switcher, using `SrvWindow::GetIcon()`. See `server/server.cpp:380-436`.
- **The Dock** learns about windows through events, not polling. Each normal window registers `os/Window/0x<address>` and posts title, flags, desktop mask, visibility, minimized state and icon (`server/swindow.cpp:119-171`).

### 1.6 Virtual desktops

There are 32 desktops in a fixed array (`server/desktop.cpp:103-112`). Each has its own window list, system-window list, active window, focus stack, screen mode and maximum window frame. So each desktop can have its own resolution and colour depth.

Windows belong to desktops through a 32-bit mask (`m_nDesktopMask`, `os::ALL_DESKTOPS` for all). A mask of 0 means the current desktop (`server/swindow.cpp:102-109`). Per-desktop frames are in `m_asDTState[32]`, and with `WND_INDEP_DESKTOP_FRAMES` a window keeps a separate position per desktop (`server/desktop.cpp:620-630`).

**Only the active desktop's windows are in the tree.** `set_desktop()` (`server/desktop.cpp:581`) does this:
1. Removes every layer from `g_pcTopView`, saving z-order and positions.
2. Changes screen mode if needed (`setup_screenmode`, `:641`).
3. Adds the new desktop's windows, from the saved list.
4. If the new resolution is smaller, moves windows that would be off-screen (`:664-690`).
5. Sends `M_DESKTOP_ACTIVATED`, `M_SCREENMODE_CHANGED` and `M_WINDOW_FRAME_CHANGED`.
6. Restores focus, or focuses the Desktop app if nothing was active (`:717-727`).
7. Deletes regions for windows not shown any more (`:734-738`).

Windows on other desktops have no regions, so what they draw meanwhile is dropped, and they get `M_PAINT` when they return. `RemoveChild` keeps backbuffers, but step 7 still deletes the regions, so by reading, a window repaints on return in mode B as well, unless it is also on the new desktop.

`AppServer::SwitchDesktop()` (`server/server.cpp:1096`) wraps this. Alt+F-key while dragging takes the window along. Holding button 2 copies it to the new desktop instead of moving it (`:1107-1116`).

## 2. How the registrar is used

The registrar is **not** an app roster like BeOS's. Despite the name, it doesn't track running apps or windows. It's a **per-user file-type database and application list**, running as its own process.

- Server: `registrar/registrar.cpp`. A normal `os::Application` named `registrar`, with no windows normally. Its message port is `l:registrar`, made public at `:1524`.
- Client API: `os::RegistrarManager` (`include/storage/registrar.h:128`, `libsyllable/storage/registrar.cpp`).
- Started by `start.sh:47`. It is restarted in-process by the `while(1)` in `main()` (`registrar/registrar.cpp:1535-1542`), and it refuses to quit (`OkToQuit()` returns false, `:1530`).

What it stores (`registrar/registrar.h`): per user, a list of types (MIME type, description, icon, extensions, handlers, default handler) and an application list.

Connection flow: `RegistrarManager::Get()` is reference counted. The constructor finds `l:registrar` and sends `REGISTRAR_LOGIN` with port, `$USER` and process id (`libsyllable/storage/registrar.cpp:223-260`). The registrar loads `/home/<user>/Settings/registrar` once per user and counts clients (`registrar/registrar.cpp:155-250`). It saves on logout only (`:348`). Root's database loads at startup (`:95`). `system/image/registrar-types.json` holds the seed data for the image.

What it answers:
- **Type management:** `REGISTRAR_REGISTER_TYPE`, icon, extension, handler, default handler (`registrar/registrar.cpp:481-860`). Apps self-register, for example `RegisterType()` then `RegisterAsTypeHandler()`.
- **`GetTypeAndIcon()`** (`:885`): the most used call. Decides a file's type from its `os::MimeType` attribute, executable bit, directory, disk device or extension, and picks an icon. The `os::Icon` attribute wins (`:1117`). This is what `icondirview.cpp` uses, so the Desktop and file browser depend on it.
- **`GetAppList()`** (`:1171`): scans application folders (`ScanAppPath`, `:1202`), reading `os::Category` attributes, and watches them with node monitors. On a change it posts event `os/Registrar/AppList` (`:98`, `:1511`). The Dock's menu listens for it (`system/apps/desktop/dock/Dock.cpp:495-497`).
- **`Launch()`** runs **in the client**, not the server (`libsyllable/storage/registrar.cpp:1035`). It asks for the type, then runs the file or its handler.

Users of it: about 24 source files, including Dock, Desktop/file views, the FileTypes prefs, aedit, aview, `open`, Whisper, ColdFish, Webster and the assistant.

Link to the appserver: there is only one, and it's a hack. When a process dies the kernel tells the appserver (`-1` message, `system/sys/kernel/kernel/fork.c:133`). `AppServer::R_ClientDied()` then finds `l:registrar` and forwards a code `-1` message so the registrar can drop that client (`server/server.cpp:266-277`, `registrar/registrar.cpp:1494`, `:1384`). The comment says `FIXME: Switch the registrar to the os::Event interface`, which is easy because `os/System/ProcessHasQuit` already exists (`server/server.cpp:210`, `:280`).

`CloseWindows()` skips `registrar`, `media_server`, the Desktop and the login app during shutdown, matching by name (`server/server.cpp:1043-1046`).

## 3. How to add other servers

### 3.1 The established pattern

The media server and the registrar work the same way (`system/sys/media/mediaserver/mediaserver.cpp:34`, `:985-1006`). A new server copies this:

```cpp
class MyServer : public os::Application {
public:
    MyServer() : Application( "my_server" ) {}          // port is "l:my_server"
    thread_id Start() {
        make_port_public( GetMsgPort() );               // findable by name
        return Application::Run();
    }
    bool OkToQuit() { return false; }
    void HandleMessage( os::Message* pcMsg );           // your protocol
};
int main() {
    while( 1 ) { MyServer* p = new MyServer(); p->Start(); }
}
```

Steps:
1. **Code:** add a directory such as `system/sys/servers/my_server/`, with `syllable_program(my_server ... LIBS libsyllable INSTALL system STRIP)` in its CMakeLists.txt. See `server/CMakeLists.txt`. Add it to `system/sys/servers/CMakeLists.txt`.
2. **Client library:** like `libmedia/manager.cpp:200` or `RegistrarManager`, call `find_port( "l:my_server" )` and wrap a `Messenger`. Keep message code numbers in one public header.
3. **Boot:** add `/system/my_server > /dev/null &` to `system/image/system/start.sh`. If it's optional, use a script in `system/image/system/index/tasks/start/`, which `start.sh` sources (`:77-85`).
4. **Shutdown:** add its name to the exclusion list in `CloseWindows()` (`server/server.cpp:1043-1046`). Otherwise logout sends it `M_QUIT`.
5. **Client cleanup:** subscribe to `os/System/ProcessHasQuit`, don't copy the registrar's `-1` hack.
6. **Change notification:** register events with `os::Event::Register( "os/MyServer/...", ... )`.
7. **Shared memory for large data:** use areas, as the media server does (`mediaserver.cpp:644`).

Ports are one per name; a duplicate `make_port_public()` returns `-EEXIST` (`system/sys/kernel/kernel/msgport.c:590-595`).

### 3.2 The catch: `Application` needs the appserver

`Application::Application()` connects to the appserver in its constructor (`libsyllable/util/application.cpp:60-100`), and `sys_get_app_server_port()` blocks until the appserver has published its port (`system/sys/kernel/kernel/kernel.c:78`). That's fine for the registrar and media server. It would deadlock only if the appserver waited for that server during startup, because the appserver publishes its port after `InitInputSystem()` (`server/server.cpp:864-866`).

The options are:
- **Recommended for the input server:** use a plain `Application( "input_server" )` and make sure the appserver never waits for it. The bridge in 3.3 does this by starting a thread and returning at once. Input is simply absent until the server connects.
- Base it on `os::Looper` plus a public port, not `Application`, if the server must run before the appserver is up.
- Add `Application( name, APP_SERVER )`. `inputserver.cpp:48` calls this, but the constructor doesn't exist in `libsyllable`, and nothing defines what it should do. It's one of several reasons the branch doesn't compile (see 3.3).

Also note that a server dying is not noticed by clients today, and the kernel's thread-death notification goes only to the appserver (`fork.c:133`).

### 3.3 Input server: what the appserver must change

Today the appserver does all of this in-process:
- `InitInputSystem()` loads plugins from `/system/drivers/appserver/input/` (`server/input.cpp:279`). Each exports `init_input_node`, `get_node_count` and `get_input_node`.
- Drivers queue events with `InputNode::EnqueueEvent()` (`server/input.cpp:130`), which adds qualifiers and a timestamp.
- `InputNode::EventLoop()` (`server/input.cpp:212`) dispatches keys to `HandleKeyboard()` and mouse events to `SrvWindow::HandleInputEvent()`.
- Key repeat, qualifiers and keymap lookup live in `server/keyboard.cpp`.
- Hotkeys are in `AppServer::SendKeyCode()` (`server/server.cpp:297-437`).

Recommended, lowest-risk design:
1. Add one appserver input plugin, say `drivers/input/inputserver_bridge`, that makes a public port, reads from it on its own thread, and calls `EnqueueEvent()` with the same messages drivers send today (`M_KEY_DOWN`/`M_KEY_UP` with `_key`, and the mouse messages). `Start()` must return at once so startup can continue. Nothing else in the appserver needs to change.
2. Point the input server at that port. Today `InputServer::Start()` sends every event to the appserver's main port (`inputserver.cpp:116`), where the main loop's `default:` case logs "Unknown command" and drops it (`server/server.cpp:1019`).
3. Start the input server from `start.sh` before `dlogin`, because the login window needs the keyboard. The appserver is already running by then.
4. Remove the old `keyboard`, `ps2mouse`, `usbmouse` and similar plugins from `/system/drivers/appserver/input/` once the bridge works. Keep them available as a fallback, because if the input server dies you lose the keyboard and mouse.
5. Keep hotkeys, focus and window dispatch in the appserver; only it knows the window tree.
6. Later, send already-translated key events, moving `convert_key_code()` to the input server, and add input methods and filters.

On `feat/input-server` today (checked at 7e122756):
- The code is mostly copied from the appserver and doesn't compile yet, because of `APP_SERVER`, `os/`-style headers in `inputserver/server/` and `AppserverConfig`. `inputserver/server/keyboard.cpp:338` still refers to the appserver's config class.
- Nothing there builds yet. `inputserver/server/CMakeLists.txt` is a blank line, and the `CMakeLists.txt` files in `devices/*` are empty.
- `inputserver.cpp:104` uses `find_port( "l:is_test_app" )` in debug builds only, as a test stand-in for the appserver.
- `M_INPUT_*` and `M_SERVER_*` exist in `messages.h` on that branch, but the appserver doesn't handle them.
- `AttrBundle_s` is now public and `AttributeSettings::ListSettings()` exists, which closes two gaps from the earlier audit. See Section 7.2 for a new problem in that change.
- `system/sys/kernel/kernel/services.c` is on the branch but isn't in any kernel build file, so that service manager isn't built.
- See `docs/keyboard-numlock-super-key.md` for its keyboard gaps.

### 3.4 Font server: harder, and I'd keep the rendering in the appserver

`FontServer` (`server/sfont.h:203`) is a class inside the appserver, not a process. It owns FreeType, scans `/system/fonts/`, and is rescanned by a node monitor (`server/server.cpp:868-873`, `:1011`). Glyph bitmaps live in the appserver's heap, and `DisplayDriver::RenderGlyph()` (`server/ddriver.cpp:1750`) reads them directly while drawing. Text measurement (`AR_GET_TEXT_EXTENTS`, `AR_GET_STRING_WIDTHS`, `server/sapplication.cpp:1308`, `:1368`) runs on the app's server thread.

A separate font process would put a message round trip into every text draw, unless glyphs were in shared memory. So there are two options:
- **Recommended: a font *management* service.** Install and enable fonts, track metadata, and tell the appserver to rescan with the existing `DR_RESCAN_FONTS` (`server/server.cpp:902`). Rendering stays put. This follows the pattern in 3.1 with no appserver changes.
- **Full split:** the font server owns FreeType and fills a shared-memory glyph cache keyed by font instance and glyph; the appserver maps that area and only asks over a port on a miss. `Glyph` and `FontNode` would need to stop using heap pointers. It's a big change, and the reason to do it would be isolating crashes in FreeType.

The `cairo` branch makes this more complicated, because it adds a second text path inside the appserver: Pango and fontconfig for `GFX_DRAW_TEXTLAYOUT`, next to `FontServer` for old drawing. A font service should plan for both.

## 4. Decorators

### 4.1 How they work

A decorator is a plugin that draws the window frame and says what is clickable. It runs **inside the appserver process**, so a crash in one takes the whole GUI down.

- **Files:** `/system/drivers/appserver/decorators/<name>`, selected by `WndDecorator =` in `/system/config/appserver` (the image uses Photon). `DefaultDecorator` is built in as the fallback (`server/server.cpp:185-195`).
- **Exports:** `get_api_version()` must equal `WNDDECORATOR_APIVERSION`, now 6 (`server/windowdecorator.h`). `create_decorator( Layer*, uint32 nFlags )` is required. `unload_decorator()` is optional (`server/server.cpp:90-144`). A version mismatch rejects the plugin (`:119`, `:123`).
- **Interface:** `HitTest`, `FrameSized`, `GetBorderSize`, `GetMinimumSize`, `SetTitle`, `SetFlags`, `FontChanged`, `SetWindowFlags`, `SetFocusState`, `SetButtonState`, `Render` (`server/windowdecorator.h`).
- **Lifecycle:** `SrvWindow` creates one per window (`server/swindow.cpp:87`). Changing the decorator loads the new plugin, replaces it on every window (`server/sapplication.cpp:222`), then unloads the old one (`server/server.cpp:153-172`). Change it at runtime with `DR_SET_WINDOW_DECORATOR` (`server/server.cpp:983`).
- **Drawing:** the decorator draws on its `GetLayer()` (the `WndBorder`) with `SetFgColor`, `FillRect`, `DrawLine`, `DrawString` and `DrawBitMap`. `WndBorder::Paint()` calls `Render()` (`server/wndborder.cpp:364`).
- **Build:** `syllable_plugin(deco_x OUTPUT_NAME X SOURCES ... INCLUDES .../server INSTALL system/drivers/appserver/decorators)`, and add it to `decorators/CMakeLists.txt`. Blue and Winter are in CMake but not in the old `decorators/Makefile` `SUBDIRS`.

There are 11 plugins: amiga, beish, Blue, drow, mulvane, next, photon, red, warpspeed, win98 and Winter. Only **Blue** and **Winter** use bitmaps.

### 4.2 How bitmaps work today (Winter)

All steps are in `decorators/Winter/WinterDecorator.cpp`:

1. Pixel data is compiled in as a C array of comma-separated `R,G,B` bytes: `#include "pixmaps/normal/buttonClose.h"` (`:582-622`).
2. On the first `create_decorator()`, it creates server bitmaps once and keeps them in file statics: `g_pcButtonClose = new SrvBitmap( 44, 22, CS_RGB32 );` (`:625-637`).
3. `LoadBitmap()` copies RGB into BGRA with alpha fixed at 0xff (`:67-81`, with the comment `// Should use ALPHA`).
4. `Render()` draws them:

```cpp
pcView->SetDrawingMode( DM_OVER );
pcView->DrawBitMap( g_pcButtonClose, Rect( 0, 0, 43, 21 ), cRect );
pcView->SetDrawingMode( DM_COPY );
```

5. `unload_decorator()` calls `Release()` on each one (`:660-677`).

Rectangles are inclusive, so 44 wide is `0` to `43`. `SrvBitmap` allocates its own memory and can be any size. The blitter converts `CS_RGB32` to the screen format, including RGB16.

How transparency works now: `DM_OVER` treats exactly `0xffffffff` as transparent (`TRANSPARENT_RGB32`, `include/gui/guidefines.h:91`, used in `blit_convert_over`, `server/ddriver.cpp`). Because `LoadBitmap()` sets alpha to 0xff, **pure white pixels in Blue and Winter art become see-through**. It isn't real alpha.

Drawing mode behaviour (`DisplayDriver::BltBitmap`, `server/ddriver.cpp:1704`):

| Mode | Transparency | Scaling |
|---|---|---|
| `DM_COPY` | None | Yes |
| `DM_OVER` | One key colour | **No** |
| `DM_BLEND` | Full 8-bit alpha for RGB32/RGBA32 sources | Yes |

### 4.3 How to attach bitmaps or images

**Option 1: real alpha in compiled-in art.** Small and safe. Generate 4-channel arrays, copy alpha in `LoadBitmap()`, and draw with `DM_BLEND`. Antialiased buttons and shadows then work against the title bar. This works over the frame only, not over what's behind the window (see option 5).

**Option 2: load art from files, so themes don't need a rebuild.** Don't use `os::Bitmap`, `os::BitmapImage` or `os::Image`. They call `Application::GetInstance()` (`libsyllable/gui/bitmap.cpp:53`), and there is no client `Application` inside the appserver. Use one of these:
- Simple: read a raw file with `fopen()`:

```cpp
SrvBitmap* LoadTheme( const char* pzPath, int w, int h )
{
    FILE* f = fopen( pzPath, "r" );
    if( f == NULL )
        return NULL;
    SrvBitmap* b = new SrvBitmap( w, h, CS_RGB32 );
    fread( b->m_pRaster, 1, b->m_nBytesPerLine * h, f );   // BGRA, top row first
    fclose( f );
    return b;
}
```

Keep the art as `.png` in the source tree and convert it to raw at build time. Put it in something like `/system/resources/decorators/<name>/`. Fall back to compiled-in art, because a missing file must not leave windows with no frame.

- Decode PNG directly with libpng linked to the plugin. This needs care, because plugins link with `-nodefaultlibs` (`cmake/Syllable.cmake:176`), and libpng isn't currently in the appserver (`screenshot.cpp` uses it but isn't in `SOURCES`).
- If the `cairo` work lands, the appserver links cairo, and `cairo_image_surface_create_from_png()` becomes an option, because the cairo port is built with libpng (`cmake/ports/30-cairo.cmake`). Cairo's alpha is premultiplied, so check that against `blit_convert_alpha` first.

**Option 3: scalable frames (nine-slice).** For a stretchable title bar, draw left and right caps with `DM_BLEND`, and stretch the centre slice with `DM_COPY` or `DM_BLEND`. The old `decorators/Winter/pixmaps/decor/stretch.h` suggests this was once planned. Repaint only what `Render()`'s `cUpdateRect` asks for, because it runs on every damaged region.

**Option 4: per-window images, such as the app icon in the title bar.** The server already stores the icon: `SrvWindow::GetIcon()` (`server/swindow.h:102`), set through `Window::SetIcon()`, `WR_SET_ICON` (`server/swindow.cpp:1751`). Notes:
- It stores only bitmaps created with `Bitmap::SHARE_FRAMEBUFFER` (`:1778`).
- `WR_SET_ICON` doesn't currently redraw the frame (`:1788` only posts the event).
- A decorator can reach it now with `GetLayer()->GetWindow()->GetIcon()`, and `WinSelect` shows how (`server/winselect.cpp:320`).
- Cleanest change: add `virtual void SetIcon( SrvBitmap* )` to `WindowDecorator`, call it from `WR_SET_ICON` and `ReplaceDecorator()`, and bump `WNDDECORATOR_APIVERSION` to 7. Adding a virtual function changes the plugin ABI, so all decorators must be rebuilt together.

**Option 5: non-rectangular frames.** `Layer::SetShapeRegion()` (`server/layer.cpp:978`) gives rounded corners. No decorator uses it today, and `WndBorder::SetDecorator()` clears it (`server/wndborder.cpp:231`). It's a yes/no pixel mask, so there are no soft edges. **Real translucency isn't possible**, such as soft shadows or see-through title bars, because there's no compositor: the server doesn't blend windows together. Mode B's backbuffers are the starting point for one.

Rules for decorator code:
- `Render()` runs with the layer gate held, so don't do file I/O or slow work there. Load in `create_decorator()`.
- Keep bitmaps in statics shared by all windows, and release them in `unload_decorator()`.
- Handle `FontChanged()`, `SetFlags()` and every `WND_NO_*` flag.
- Check NULLs. A bad pointer kills the GUI.

## 5. Other things worth knowing

### 5.1 The app protocol

`Application` asks for a session with `DR_CREATE_APP`, and a `SrvApplication` thread and port come back (`server/server.cpp:461`). `AR_*` messages go to that thread, and `WR_*` go to a window's port. Drawing is batched in `WR_RENDER` packets (`SrvWindow::R_Render`, `server/swindow.cpp:582`). The port table is in `include/appserver/protocol.h`.

Shared bitmaps are areas mapped by both sides (`server/sapplication.cpp:352`). With `SHARE_FRAMEBUFFER`, apps write pixels directly and the server blits them, which is how video and fast image updates work. Video overlays have their own hardware path (`WR_CREATE_VIDEO_OVERLAY`, `server/sapplication.cpp:1122`).

Text drawing and `Layer` drawing are all server-side, with no client rendering step.

### 5.2 The event bus

`SrvEvents` (`server/event.cpp`) is a small publish/subscribe system in the appserver, used through `os::Event`. It's a tree of string IDs such as `os/System/PrintKeyWasHit`. Any process can register, monitor or call one. It keeps the last message, so a new subscriber can read current state, and it cleans up after dead processes (`:639`).

This is the best integration point for new servers. It is also how the shortcut system works: `Application::RegisterKeyEvent()` binds a key to an event (`server/server.cpp:297-308`).

### 5.3 Drivers and graphics

- `init_desktops()` tries each `/dev/graphics/*` node, asks it which appserver driver to load, and falls back to the built-in VESA driver (`server/desktop.cpp:769-877`).
- A driver subclasses `DisplayDriver` (`server/ddriver.h:49`), providing mode setting and optionally `FillRect`, `BltBitmap` and `DrawLine` acceleration. Software versions are the defaults.
- Video memory is managed by `server/ddmemory.cpp`.
- The cursor is a sprite (`server/sprite.cpp`) that is hidden around overlapping screen draws, unless the driver has a hardware cursor.

### 5.4 Quick wins found by reading (none tested)

1. **Modal and front-most flags do nothing.** `WND_MODAL`, `WND_FRONTMOST` and `WND_SEND_WINDOWS_CHANGED` are defined in `include/gui/window.h:50-53` but have no server code. Dialogs that set `WND_MODAL` don't block their parent and can go behind it.
2. **Shaped windows in mode B.** `TopLayer::RebuildRegion()` subtracts siblings by rectangle, not shape, and ignores `WID_TRANSPARENT` (`server/toplayer.cpp:430-434`). In mode B, a shaped window should still hide the full rectangle under it.
3. **Per-desktop backdrops aren't stored.** `set_desktop_config()` ignores `cBackDropPath` (`server/desktop.cpp:115-121`), so `get_desktop_config()` always returns an empty path.
4. **`DR_SET_DESKTOP` can crash the appserver** during a window drag or a titlebar click. A 2006 comment admits this (`server/server.cpp:655-665`). The Dock and the switcher use this path.
5. **Registrar:**
   - `Login()` has no `return` after its error reply, and it carries on with missing fields (`registrar/registrar.cpp:297-301`).
   - `mkdir( zPath, 700 )` is decimal, so it should be `0700` (`:115`).
   - `ProcessKilled()` loops over `m_cUsers` and `m_cClients` while `Logout()` erases from them (`:1395-1411`).
   - The database saves only on logout, so a crash loses changes.
6. **Screenshot code isn't built.** `screenshot.cpp` isn't in `SOURCES`, and `__NO_SCREENSHOT_SUPPORT__` is set (`server/CMakeLists.txt`). Print Screen only posts `os/System/PrintKeyWasHit`; some other app has to act on it.
7. **`keyb_thread` is dead code.** `InitKeyboard()` spawns `HandleKeyboard` with a NULL argument (`server/keyboard.cpp:407`), so it does nothing and exits. The real work is on `input_thread`.
8. **Decorators can't use alpha** until `LoadBitmap()` stops forcing 0xff.

### 5.5 Bigger directions

- **System-memory backbuffers for every driver.** Turn on the `#if 0` fallback, with testing. Uncovering a window then wouldn't need an app repaint, even under VESA and QEMU. This is the highest-value change here, and it's also the base for a compositor. The code looks like a debug aid (its comment says to enable it to test), and nothing shows it has been run in this tree. Check these first:
  - **Memory:** each window costs width × height × bytes per pixel in RAM, plus the 11-pixel oversize (`server/toplayer.cpp:79`).
  - **Freeing:** `SrvBitmap` allocates with `calloc()` but frees with `delete[]` (`server/bitmap.cpp:73`, `:93`). This path would use that mismatch all the time.
  - **Flags:** the video-memory path sets `NO_ALPHA_CHANNEL` and this one doesn't, so blending into a backbuffer could behave differently.
  - **Mode changes:** `TopLayer::FreeBackbuffers()` (`server/toplayer.cpp:142`) was written for video memory, so test desktop and resolution switches.
  - **Rounded windows:** a shaped window that keeps `WND_SINGLEBUFFER` keeps its corners but skips the backbuffer. Without that flag the corners should go square until Section 5.4 item 2 is fixed.
- **A compositor.** With every window in a backbuffer, the screen becomes a blend of those buffers. That gives shadows, translucency and animation. Moving screen updates to one thread would help a lot.
- **Visibility messages.** Telling apps they're fully covered would save work in video and animation.
- **Isolate decorators.** They run in the server and any bug kills the GUI. Data-driven themes (option 2 in Section 4.3) reduce the amount of plugin code.
- **Move the registrar to `os::Event`** and remove the name-matching lists in `CloseWindows()`. A "this server stays running" flag on `Application` would replace the hardcoded names.

### 5.6 QEMU video

There's no QEMU-specific driver, and it isn't a concern for now. Tested on 2026-11-04 by booting `out/syllable.qcow2` at both settings and checking the serial log and a screenshot about 150 seconds in:

| QEMU setting | Driver used | Result |
| --- | --- | --- |
| `-vga std` (default, what `sb run` uses) | Built-in VESA, after the `vmware` kernel driver finds no card | Finds 47 usable modes and reaches the login screen at 800x600 |
| `-vga vmware` | `vmware` | Loads, then the screen stays on QEMU's "Guest has not initialized the display" message, with only a scrap of the login screen showing |

The VMware driver isn't a usable alternative as it stands. It accepts only the host colour depth, which is 32-bit under QEMU, so it refuses the default 800x600 16-bit mode and the server drops to 640x480 32-bit. The blank screen's cause wasn't diagnosed. Even when working it wouldn't add backbuffers, because it doesn't call `InitMemory()`, and its only acceleration is screen-to-screen copy (`drivers/video/vmware/vmware.cpp:419`, `:467`).

The real gap is test coverage. VESA has no backbuffers, so QEMU never runs mode B, including the path where shapes look to be ignored (Section 5.4 item 2). A QEMU driver with `InitMemory()` would cover it, but the system-memory fallback in Section 5.5 covers the same code and also helps every real card without backbuffers. Do that first, and write a QEMU driver only if better resolution switching becomes worth it.

### 5.7 Key maps: Num Lock and the Super key

Key maps are text files in `system/other/keymaps/*.map`, compiled by `keymapconv` to `/system/keymaps/`, 173 in the current build. By reading, neither feature below can be fully enabled by a setting today. Nothing here has been built or tried.

**Num Lock on start.** The setting exists: uncomment `InitialLocks = NumLock` (`American.map:6`), then rebuild. On its own that's a trap, because it only lights the LED. `KeyboardDriver`'s constructor reads `m_nLockSetting` for the LED (`drivers/input/keyboard/keyboard.cpp:76`), but the real state, `g_nLock` (`server/keyboard.cpp:50`), starts at 0 and never reads it. The keypad stays off and the LED is backwards from then on.
- **Fix:** set `g_nLock = g_psKeymap->m_nLockSetting` once at startup, for example on the first call to `HandleKeyboard()` (`server/keyboard.cpp:322`), about 4 lines.
- **One lock only:** the parser reads just the first word after `InitialLocks =` (`keymapconv.cpp:206`), and the driver's `switch` handles only one lock.
- **Per key map:** another layout needs the same line, and a whole map has to be rebuilt to change it.
- **USB keyboards:** they'd get the state but no LED, which is still a TODO (`drivers/input/usbkeyboard/keyboard.cpp:77`).

**Super key.** A key map can't do this. Its `LCommand`, `RCommand` and `Menu` fields are stored but the server never reads them. Qualifiers are hardcoded in `SetQualifiers()`, and the keys are dropped in the kernel before any key map sees them. It needs four changes:
1. **Kernel:** map the keys to the raw codes already reserved for them, 0x66 and 0x67, and 0x68 for Menu (see the diagram at `server/keyboard.cpp:54-77`). That's `kernel-drivers/keyboard/kbd.c:221-223` for PS/2 and entries 125-127 at `kernel-drivers/input/usb_hid/hid.h:342-344` for USB.
2. **Qualifiers:** add `QUAL_LSUPER = 0x80`, `QUAL_RSUPER = 0x200` and `QUAL_SUPER` to `include/gui/guidefines.h:152-168`. 0x40 is `QUAL_REPEAT` and 0x100 is `QUAL_DEADKEY`, so 0x80 is the last free bit below them.
3. **Server:** add the press and release cases to `SetQualifiers()` (`server/keyboard.cpp:89`).
4. **Shortcuts:** add `SUPER+` to the parser and the match in `libsyllable/util/shortcutkey.cpp:13-21` and `:70-72`.

The key map's entries for these keys are 0, so they'd produce no character, which suits a pure qualifier. System-wide shortcuts such as Super+Tab would go in `server/server.cpp` next to the Alt ones (`:412`). `feat/input-server` has its own copy of this code, so the server changes would have to go there too (Section 7.2).

## 6. Rounded, borderless and antialiased windows

Added after the review, from follow-up questions. The code and designs here have not been compiled or run. No app in the tree uses a shape yet, and the real Dock is a plain rectangle (`system/apps/desktop/dock/Dock.cpp:405`).

### 6.1 A borderless window with rounded corners

Create the window with `os::WND_NO_BORDER`, give it a rounded shape with `Window::SetShapeRegion()` (`libsyllable/gui/window.cpp:810`), and rebuild the shape on every resize. A region is a list of rectangles, so the curve is built one pixel row at a time:

```cpp
static os::Region MakeRounded( int w, int h, int r )
{
    os::Region cReg;
    for( int y = 0; y < r; ++y )
    {
        float d = r - y - 0.5f;
        int nInset = int( r - sqrtf( r * r - d * d ) + 0.5f );
        cReg.AddRect( os::IRect( nInset, y, w - 1 - nInset, y ) );
        cReg.AddRect( os::IRect( nInset, h - 1 - y, w - 1 - nInset, h - 1 - y ) );
    }
    cReg.AddRect( os::IRect( 0, r, w - 1, h - 1 - r ) );
    return cReg;
}

class DockWindow : public os::Window
{
public:
    DockWindow( const os::Rect& cFrame )
        : os::Window( cFrame, "my_dock", "Dock", os::WND_NO_BORDER | os::WND_SINGLEBUFFER, os::ALL_DESKTOPS )
    {
        UpdateShape();
    }
    void FrameSized( const os::Point& cDelta )
    {
        os::Window::FrameSized( cDelta );
        UpdateShape();
    }
private:
    void UpdateShape()
    {
        os::Rect cB = GetBounds();
        SetShapeRegion( MakeRounded( int( cB.Width() ) + 1, int( cB.Height() ) + 1, 10 ) );
    }
};
```

What to watch for:
- **Keep `WND_SINGLEBUFFER`.** In mode B the screen blit looks like it ignores the shape (see 5.4 item 2), so the corners would stay square. Under VESA or QEMU the flag changes nothing.
- **Corners are hard-edged.** A shape is yes or no per pixel, and the server doesn't blend windows.
- **Clicks in the cut corners still reach the window.** `Layer::GetChildAt()` tests the full rectangle (`server/layer.cpp:487`), so ignore them in `MouseDown`.
- **`WND_NO_BORDER` windows don't post `os/Window/` events** (`server/swindow.cpp:119`), so a dock won't list itself or other borderless windows.

For a real dock:
- Reserve screen space with `DR_SET_DESKTOP_MAX_WINFRAME`, as `Dock.cpp:1155` does, so maximized windows stop short of it.
- Expect other windows to be able to cover it, because `WND_FRONTMOST` has no server code.
- Add your own drag handling if it should move, because there's no frame to grab. Call `GetWindow()->Flush()` after `MoveBy()`: `Window::SetFrame()` flushes but `MoveBy()` doesn't (`libsyllable/gui/window.cpp:664-679`), so without it the window stays still until something else flushes.

The same code is now a demo in the tree, `system/apps/demos/rounded`, listed under Demos in the Dock menu. Tested on 2026-11-03 in QEMU on the VESA path (`rounded-test/rounded.cpp`, built with g++ 4.1.2 inside Syllable): the cut-out corners, `WND_SINGLEBUFFER` and dragging all work on the desktop, and the corners are hard-edged as expected. Dragging does leave a few stray corner pixels behind (128 pixels after one drag across the desktop icons); the cause isn't known yet. Apps that aren't repainting leave the window's old pixels behind, as mode A predicts.

### 6.2 Rounded corners without a region

Without a region the window stays a full rectangle. All an app can do is paint rounded corners and fill the leftover pixels with something that looks like the background. There's no per-window transparency or colour key.

This works when the colour behind the window is known:
- **Solid desktop colour or a known edge:** in `Paint()`, fill the view with that colour, then draw the rounded shape on top as a 32-bit bitmap with `DM_BLEND`. This gives smooth corners, which a region can't.
- **Dock against a screen edge:** only the two inner corners show, so only those need faking.

It fails over wallpaper or other windows, because the corners won't match and won't update when something moves underneath. I didn't find an app-level way to read back what's behind a window.

So a region is the only real cut-out today, and painted corners are the only smooth ones. They can be combined: use a region for the shape and paint a thin edge along the curve to hide the jaggies.

On the `cairo` branch no bitmap is needed for the painted edge, because `FillRoundRectangle()` draws antialiased curves directly (Section 7.1). It's still blending against the window's own pixels, so the known-colour limit is the same.

### 6.3 A better `os::Region` API

This can all be additive. Keep the rectangle list inside, because the appserver clips with it everywhere. The current API (`include/gui/region.h`) only works in rectangles: `AddRect()` doesn't check for overlaps, `operator=` is private, and there's no hit test, offset or region-to-region union.

```cpp
class Region {
public:
    // Shapes, so apps never touch rows
    static Region RoundedRect( const IRect& cRect, int nRadius, uint32 nCorners = CORNER_ALL );
    static Region Ellipse( const IRect& cRect );
    static Region FromAlpha( const Bitmap* pcMask, uint8 nThreshold = 128 );

    // Normal value behaviour
    Region& operator=( const Region& cReg );
    Region  operator|( const Region& cReg ) const;   // union
    Region  operator&( const Region& cReg ) const;   // intersect
    Region  operator-( const Region& cReg ) const;   // subtract

    // Queries
    bool Contains( const IPoint& cPos ) const;
    bool Intersects( const IRect& cRect ) const;
    void Translate( const IPoint& cOffset );
};
```

A rounded window then becomes one line: `SetShapeRegion( Region::RoundedRect( GetBounds(), 10 ) );`

The bigger win is one level up: add `Window::SetCornerRadius( int nRadius )` and let the server hold the radius and rebuild the shape itself.
- Apps no longer need a `FrameSized()` hook, and a resize can't briefly show the old shape.
- Decorators get rounded frames from the same code.
- `GetChildAt()` can test `Contains()`, so clicks in the cut corners go to the window underneath.

Limits to design around:
- **Mask size:** a shape goes through a render packet of 8180 bytes (`include/appserver/protocol.h:565`), about 500 rectangles at 16 bytes each. Rounded corners fit easily, but `FromAlpha()` on a detailed image may not. Above that limit `_AllocRenderCmd()` logs "packet to big" and returns NULL (`libsyllable/gui/window.cpp:1094-1118`), and `View::SetShapeRegion()` then returns without setting the shape. Sending a shared bitmap handle would avoid it.
- **Soft edges:** none of this adds antialiasing. The mode B problem still needs fixing too, so keep `WND_SINGLEBUFFER` until then.
- **Compatibility:** `Region` has no virtual functions, so adding methods shouldn't break existing binaries. Leave `m_cRects` public for now, because the appserver walks it directly.

Suggested order: `RoundedRect`, the operators and `Contains` first, then `SetCornerRadius` and the hit test.

### 6.4 Antialiasing

Real antialiased edges need the server to blend a window's edge with whatever is behind it, and nothing does that today, on `main` or on `cairo`.

**Now, with no server changes:** draw the edge as a 32-bit bitmap with alpha, using `DM_BLEND`. It's smooth, but only over a known colour, so it suits an edge-docked dock or decorator art on the frame. The blender handles RGB16 screens too, but expect banding, because the default image config is RGB16.

**With `cairo`:** content inside a window gets real antialiasing for shapes, curves and text, which is most of what apps need. The window's outer edge doesn't change: `SetClip()` affects drawing only, and the window's shape is still a `Region`.

**Why not a quick server trick:** blending edge pixels onto the screen once, at paint time, goes stale when anything underneath changes. In mode A the lower window's pixels aren't kept anywhere to re-blend from.

**Fixing the jagged corners, in order of effort:**
1. **Hide it (minutes, app only):** add a dark or matching 1-pixel border along the curve. It's still jagged up close, but much less visible against wallpaper.
2. **Avoid it where you can (small):** dock against a screen edge or over a solid colour, leave the region off the corners that show, and paint them smooth.
3. **Fix it (bigger, appserver):** the alpha windows below. This is the only option that removes the jaggies over wallpaper and moving windows.

**The proper fix is alpha windows, not a full compositor.** Replace the yes/no shape with an 8-bit coverage mask, and have the server blend edge pixels with what's below: `window × coverage + below × (1 − coverage)`. Cairo can't do this by itself, because it only sees the window's own pixels. With cairo, draw the same rounded rectangle into an 8-bit surface so the mask matches `FillRoundRectangle()`. Without it, calculate the mask per pixel using the same maths as `Region::RoundedRect()`. The steps, untested:
1. **Backbuffers for all drivers:** turn on the `#if 0` system-memory fallback in `server/toplayer.cpp:77-88`, so every window keeps its pixels.
2. **An opt-in flag:** add something like `WND_ALPHA`, with an RGBA32 backbuffer. `AllocateBitmap()` forces `NO_ALPHA_CHANNEL` today, so this needs its own path. The window has to start cleared to alpha 0.
3. **Two regions per alpha window:** an opaque part and a blended part such as the edge ring. `SetCornerRadius()` could generate both.
4. **Occlusion:** `TopLayer::RebuildRegion()` subtracts only the opaque part from windows below, so they stay visible under the edge.
5. **Compositing:** in `TopLayer::UpdateIfNeeded()`, blit opaque windows as now. For damage that touches a blended part, redraw bottom to top within that area only, using the existing `blit_convert_alpha`, or cairo's `OVER` operator if that work has landed. Any change to a lower window has to add damage to the alpha windows above it.

Normal windows stay on the fast path, so only those few edge pixels cost CPU, which matters because VESA and QEMU blend in software. The same work gives drop shadows and translucent docks, since a shadow is just a wider blended ring.

The main risks are mode changes and video memory. System-memory backbuffers also add a full copy per window, so test step 1 alone first. It's the right place to start anyway, because it stops uncovered windows from needing a repaint even without alpha.

## 7. What changes on other branches

Checked by reading the branches with git, without checking them out or building them. Sections 0 to 6 hold on both branches except where noted here. There's no `feat/cairo`; the branch is `cairo`, last changed 2026-04-26.

### 7.1 `cairo`

The branch (83e0a251) is 45 commits ahead of `main` and 31 behind. It adds a new drawing stack and leaves the window system alone. In `appserver/server/` only `layer.cpp`, `layer.h`, `swindow.cpp` and `Makefile` change, plus the new `graphics_cairo.cpp` and `.h`. `toplayer.cpp`, `desktop.cpp`, `server.cpp` and the decorators match `main`, so Sections 1 and 4 still apply, and so does `Region`.

What it adds:
- **`libos`:** a copy of libsyllable with a new `graphics::` API (`system/sys/appserver/libos/graphics/`, headers in `system/sys/include/pyro/pyro/graphics/`). Old `libsyllable` stays alongside it for old apps. The appserver links `-los` instead.
- **New paint path:** `View::Paint( const Rect&, graphics::Graphics& )`. `Window` calls it after the old `Paint()`, with a new `GraphicsView` each time (`libos/gui/window.cpp:1608`).
- **Drawing API:** `DrawRoundRectangle`, `FillRoundRectangle`, circles, curves, shapes, `SetAntiAlias`, `SetClip( Shape )`, fill and pen styles, and Pango text layouts.
- **Protocol:** `GFX_*` commands, numbered from 40000, travel in the normal render packets, so the 8180-byte limit applies to paths too. `R_Render` sends anything it doesn't know to `Layer::m_pcGraphicsCairo` (`swindow.cpp:999-1002`).
- **Server side:** each `Layer` owns a `GraphicsCairo`. `GFX_BEGIN_UPDATE` wraps the layer's bitmap in a cairo image surface, translates to the view, and clips to the layer's region rectangles (`graphics_cairo.cpp:45-130`). Occlusion and backbuffers therefore work as in Section 1. Cairo runs in software, directly on the screen in mode A.
- **Themes:** the start of `graphics::theme`. `BorderStyle` already has a radius, and `DEFAULT_THEME` points at `/system/themes/default/style.css`.

A rounded dock on this branch would keep the region from 6.1 for the shape, and paint a smooth edge inside it:

```cpp
void DockView::Paint( const os::Rect& cUpdate, graphics::Graphics& cGraphics )
{
    os::View::Paint( cUpdate, cGraphics );
    cGraphics.SetFillStyle( m_cDockFill );
    cGraphics.FillRoundRectangle( GetBounds(), 10 );
}
```

Problems to settle before relying on it:
- **Build state:** it's Makefile-only, with none of the `CMakeLists.txt` files from `main`, and the appserver links `-los`. Commit c61b1651 says it doesn't compile, and later commits don't fix that. `libos/graphics/theme/theme.cpp` has no `;` after `class Theme::Private`, and its method names don't match `theme.h`. `main` has the libraries, but `cmake/ports/30-cairo.cmake` says they're "installed, not used yet".
- **32-bit only:** `_BeginUpdate()` supports only RGB32, RGB24 and RGBA32 (`graphics_cairo.cpp:61-79`). The default image config is RGB16, so every `GFX_*` command would be dropped with an error until that changes. `CS_RGB24` is also given to cairo as a 4-byte format, which looks wrong.
- **Region handling:** `_BeginUpdate()` declares a local `pcReg` at `:94` that hides the member at `:575`. `_EndUpdate()` then calls `PutRegion()` on the member, which is never set (`:136`).
- **Hidden layers:** if `GetRegion()` returns NULL, `_BeginUpdate()` skips `cairo_save()` and hiding the cursor, but `_EndUpdate()` still restores and unhides.
- **Corners:** `_DrawRectangle()` uses `cairo_curve_to()` with both control points on the corner (`:339-356`). That's a tighter curve than a circle, so it won't line up with a true circular `Region` of the same radius. Use `cairo_arc()`.
- **Cost:** every `Layer` allocates a `GraphicsCairo`, and each view paints twice, old path then new.
- **Decorators can't use it yet.** They run in the server and draw through `Layer`. `DispatchCommand()` takes protocol packets and is the only public entry point. `GraphicsCairo` would need direct drawing methods for rounded, antialiased frames. That would be a better base for Section 4.3 options 3 and 5 than bitmaps.

Effect on the recommendations: do the `Region` work in `libsyllable` so it can be copied to `libos`, because `region.cpp` is effectively the same in both. `Region::RoundedRect()` and `cairo_arc()` should use the same radius maths, so the cut-out and the painted edge line up. System-memory backbuffers (Section 5.5) stay the first step. They also help cairo, because mode A blends straight on screen, which can flicker.

### 7.2 `feat/input-server`

The branch (7e122756) is 5 commits ahead of `main` and 0 behind. Outside `inputserver/` it changes only `system/sys/CMakeLists.txt`, `messages.h`, `attributesettings.h` and `attributesettings.cpp`, so `appserver/server/` matches `main`.

Sections 3.2 and 3.3 are updated for this branch. The Num Lock and Super key changes in Section 5.7 also apply here, because `system/sys/servers/inputserver/server/keyboard.cpp` has its own copy of `g_nLock` and `SetQualifiers()`. One new problem: `libsyllable/util/attributesettings.cpp` now defines `ListSettings()` twice.
- `:456` defines `AttrBundle_s* AttributeSettings::ListSettings()` but uses `m_pcSettings`, which belongs to `Private`. It looks like it should be `AttributeSettings::Private::ListSettings()`.
- `:655` defines `void AttributeSettings::ListSettings()`, which doesn't match the header (`attributesettings.h:76`) and returns a value from a `void`.

CMake builds that file only with `SYLLABLE_ATTRIBUTE_SETTINGS`, which is off (`libsyllable/CMakeLists.txt:31-38`), so the normal build doesn't hit this yet. The input server needs `AttributeSettings`, so the option will have to go on, and the old Makefile always builds the file. Rename the first definition, and change the second to return `AttrBundle_s*`. The comment in `CMakeLists.txt` is out of date on this branch too: the `os/` includes are gone from that file, and the input server now uses the class.
