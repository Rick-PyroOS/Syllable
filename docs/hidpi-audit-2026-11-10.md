# HiDPI for the appserver, libsyllable and the `cairo` branch

2026-11-10. A read-only audit of `main` (ca83ddf5) and `cairo` (83e0a251), both read from GitHub without building anything. Line numbers refer to those commits. Rick's Mac wasn't needed: the `cairo` branch is pushed.

## Summary

Syllable has no HiDPI support today, and no notion of DPI or scale anywhere. Every coordinate in the API is a screen pixel, and the font engine assumes 96 DPI in four hard-coded places.

The good news is that the architecture suits the macOS/Wayland model well: **apps keep drawing in logical points, and the appserver multiplies by an integer scale factor (1 or 2) on the way to the screen.** The client-server protocol already sends float `Point`/`Rect` for every drawing command. Inside the server, logical coordinates turn into device pixels at a small number of choke points: `Layer::SetFrame()`, the `IRect`/`IPoint` conversions in `render.cpp`, and the mouse position in `input.cpp`. That means:

- Existing apps run unchanged, with crisp text and lines at 2x. Their bitmaps and icons are pixel-doubled until they ship 2x art.
- Most of the work is in the server, not in hundreds of apps.
- The `cairo` branch gets vector HiDPI almost for free, from one `cairo_scale()` call in the server and one in `GraphicsImage`.

The recommended plan has 7 phases. A usable 2x desktop (Phases 0 to 3) is about **7 to 11 weeks** of focused work. Full polish, meaning 2x icons, the remaining apps and the `cairo` branch, is about **14 to 20 weeks**. Fractional scaling (1.25x, 1.5x) should wait (Phase 7).

A **"large text" setting** (Phase 1, 2 to 3 days) can ship first, independently of the rest. It is a stopgap, not HiDPI.

## 1. What exists today (main)

### 1.1 Coordinate model

| Piece | Where | State |
|---|---|---|
| Protocol | `include/syllable/appserver/protocol.h:181-320` | Render packets (`GRndSetFrame_s`, `GRndFillRect32_s`, `GRndCopyRect_s`, `GRndDrawBitmap`, `GRndScrollView_s`, `GRndInvalidateRect_s`) carry float `os::Point`/`os::Rect`. Good: no protocol change is needed to carry logical coordinates. |
| Layer geometry | `appserver/server/layer.h:216-217`, `layer.cpp:924-960` | `m_cFrame` (float) and `m_cIFrame` (int). `SetFrame()` makes `m_cIFrame` with `IRect( cRect )`, which truncates (`rect.h:240`). Visible and damage regions, clipping and `ConvertToBitmap()` all work on `m_cIFrame`. **This is the main place to apply the scale.** |
| Drawing | `render.cpp` | Each primitive converts once from float to int: `DrawLine` (`:193-194`), `DrawString` (`:311-312`), `FillRect` (`:957`), `ScrollRect` (`:1021`), `DrawBitMap` (`:1170-1171`). The second choke point. |
| Rect convention | `rect.h` | Rects are inclusive (`right = width - 1`). A logical rect `[l, r]` must map to device `[l*s, (r+1)*s - 1]`, not `[l*s, r*s]`. Getting this wrong leaves 1px seams everywhere, so it needs one helper used everywhere. |
| Screen size | `desktop.cpp`, `sapplication.cpp:942,986,1017`, `swindow.cpp:320-335` | Resolution is reported to clients in pixels (`"resolution"`, `"_new_resolution"`, `AR_LockDesktopReply_s::m_cResolution`). |
| Mouse | `input.cpp:174-196` | `s_cMousePos` is a float `Point`, clamped to `g_pcScreenBitmap` size and sent as `"_scr_pos"`. libsyllable's `Window` turns it into view coordinates with `ConvertFromScreen()` (`window.cpp:1165-1356`). One division by `s` at the source covers every client. |
| Window borders | `wndborder.cpp` (52 `IRect`/`IPoint` uses) | Drag and resize logic works in screen pixels and sends new frames back to the client. These need converting to logical. |

### 1.2 Fonts

- `sfont.cpp:78,82,133,137,209,211,269,273`: `FT_Set_Char_Size( ..., 96, 96 )` and `FT_Set_Pixel_Sizes( ..., size*96/72 )`. DPI is a hard-coded 96 in four places. There's no setting, and `AppserverConfig` has none (`config.cpp:558`, fonts store a point size only).
- `sfont.h:46-51,178`: glyph advances and bounds are integer device pixels. `Font::GetStringWidth()` on the client returns a float (`font.h:493`), so fractional logical widths (device width / 2) can pass through without an API change.
- Glyphs are rendered on the server (`Layer::DrawString`, `render.cpp:230`) with a 5-level anti-aliasing palette. At 2x the server must rasterise at `size * s` to get crisp text, and keep separate glyph caches per (size, scale).

### 1.3 Bitmaps and icons

- `os::Bitmap` has only pixel dimensions. There is no way to say "this 48x48 bitmap is 24x24 points at 2x".
- `Layer::DrawBitMap()` already supports scaled blits (`render.cpp:1172-1203`, nearest neighbour in `BltBitmap`), so pixel-doubling a 1x bitmap at 2x works today with the right destination rect.
- `BitmapImage::SetSize()` (`image.cpp:561`) scales with Lanczos3. That is useful for making 2x icons from bigger sources.
- App icons are resources named `icon48x48.png` (99 apps) and `icon24x24.png` (56); one app has `icon32x32.png`. `IconDirView` asks for 24 or 48 (`icondirview.cpp:691-695,1010,1419,1557`). At 2x the 24pt slots can use the existing 48 art, but 48pt slots need 96px art, or SVG.
- `stdbitmaps.cpp` holds hand-drawn 8x8 rasters (scrollbar arrows, check marks, menu arrows), and `gui/pixmaps/*.h` holds file-requester icons. These are pixel art that will look blocky when doubled.
- SVG: the `feat/svg` work (`/mnt/project-files/svg-work/svg-support.md`, section 2.1) renders at the file's declared size only. The translator API carries no target size, which it would need before SVG can serve as a HiDPI icon source.

### 1.4 Widgets (libsyllable)

- Most layout sizes come from font heights, which helps. But many widgets hard-code pixel sizes: the scrollbar is 16px (`scrollbar.cpp:273,885,922`), and fixed `Point( n, n )` sizes appear in `iconview.cpp` (22), `icondirview.cpp` (12), `textview.cpp` (10), `calendarview.cpp` (6) and others.
- With the server-scale model, none of that needs to change: 16 points becomes 32 pixels. It would only matter for the "large text" stopgap, or for fractional scaling.

### 1.5 Decorators

- `DefaultDecorator` (`defaultdecorator.cpp:58-75,231-296`) has 4px borders and a title bar from the font height, and draws its buttons as vectors scaled to the button rect. It will look right at 2x as long as its font scales.
- Winter, and probably several of the other ten in `drivers/decorators/`, use fixed `SrvBitmap` pixmaps (`WinterDecorator.cpp:31-41`, `Winter/pixmaps/`). They work but look blocky at 2x until they get @2x pixmaps.
- Decorators draw through the `Layer` API, so they get scaled along with everything else.

### 1.6 Mouse pointer

`g_anMouseImg` is an 11-pixel-wide RGB32 arrow (`desktop.cpp:58-62,891`, also set from `sapplication.cpp:176-208,1113`), and `wndborder.cpp:39` has 20px resize pointers. They go to `DisplayDriver::SetCursorBitmap()` (`ddriver.h:98`), so they are hardware or software sprites in device pixels. At 2x an 11px arrow is tiny. It needs 2x images, or doubling at upload time.

### 1.7 Apps that touch pixels directly

These see device pixels and need an opt-out or a fix:

- `Desktop::GetFrameBuffer()` users: the dock Camera plugin, `bzoom`, and the screen media driver (`sys/media/drivers/screen/screen.cpp`).
- Video overlays (media player). Their rects are in screen pixels.
- SDL and Mesa ports (`Builder/packages/sdl-*`, `mesa-*`). They are fine if they keep drawing into a `Bitmap` that gets pixel-doubled, since games rarely want 2x.
- 30 app directories use `LockRaster()`/`SHARE_FRAMEBUFFER` (dock, Webster's graphics layer, Whisper, xpdf, CodeEdit, AttribEdit, and others). Writing into a `Bitmap` and drawing it is fine under the scale model, because those bitmaps just get doubled. They only need work to become sharp.
- Webster today is WebKit r29946 with a native `os::View` backend. It will look like any other 1x app, with crisp text through the server, since it draws text with `DrawString`. Real 2x web content comes with the WebKit upgrade (`docs/webster-webkit-upgrade.md`), whose Cairo backend has a device scale factor.

### 1.8 Video drivers and testing

HiDPI needs big modes. `vesadrv.cpp:126-138` takes whatever the VGA BIOS lists. Under QEMU that's the only practical way to get 2560x1600 or 3840x2160 today, apart from the VMware SVGA driver. The Bochs DISPI driver in `docs/spice-qxl-driver-plan-2026-11-10.md` (2 to 4 days, any resolution, EDID) is the right test vehicle and should come first. On the Mac, QEMU's Cocoa display with `zoom-to-fit=off` on a Retina screen shows a 2560x1600 guest at real density.

## 2. The `cairo` branch

What's there (see also `docs/rich-text-editor-design.md` sections 1 and 13):

- **Server path:** `appserver/server/graphics_cairo.cpp`. For each update, `_BeginUpdate()` wraps the layer's `SrvBitmap` in a cairo image surface (`:60-76`), translates to the view origin with `ConvertToBitmap()` (`:89-91`) and clips to the layer's region rects (`:100-108`). Paths, fills, bitmaps (`:453-466`) and Pango text layouts (`:436`, `:517-527`) all go through cairo.
- **Client path:** `libos/graphics/graphics_image.cpp:95-103`. `GraphicsImage` wraps an `os::Bitmap` in a cairo surface inside the app.
- **libos widgets:** `libos/gui/button.cpp:325-372` and `view.cpp:2060` paint with `graphics::Shape`/`PenStyle`/`FillStyle` and theme styles, which are resolution-independent vectors.
- `GraphicsImage::CreateFromIconset()` is a stub (`graphics_image.cpp:60`). Someone planned multi-size icon sets.
- Libraries: cairo 1.4.14, Pango 1.20.2, FreeType 2.3.9 (`cmake/ports/30-cairo.cmake`).

What HiDPI needs on the branch:

1. **Server scale is one line in the right place.** In `_BeginUpdate()`, keep setting the clip in device pixels, which the region rects already will be after Phase 2. Then call `cairo_scale( psCairo, s, s )` and translate by the *logical* origin. Every vector, path and Pango layout then renders at device resolution. Bitmap patterns (`:461-466`) are filtered by cairo, which is smoother than the main branch's nearest-neighbour doubling.
2. **Pango.** Set `pango_cairo_context_set_resolution()` to 96 (logical) and let the CTM do the scaling. Set `cairo_font_options` hinting explicitly, so that the client (`textlayout.cpp:45-53`, which uses default options and no `pango_cairo_update_context()`) and the server measure the same way. That is the same mismatch the rich-text doc flags in section 1.3, and at 2x it gets worse.
3. **Client `GraphicsImage`.** Make the bitmap at `size * s` pixels and `cairo_scale( s, s )` on the context. Cairo 1.4 has no `cairo_surface_set_device_scale()` (added in 1.14), so the scale lives on the context and must be reapplied after `cairo_identity_matrix()` or `SetTransform()` (`GraphicsBackend::ResetTransform/SetTransform` in `graphics_backend.h`). **Upgrading cairo to 1.14+ removes this whole class of bug** and is worth doing alongside the GLib/Pango upgrades already discussed.
4. **Existing bugs that get worse at 2x:**
   - `GraphicsImage` sizes the surface with `GetBounds().Width()`, one pixel short because rects are inclusive (`graphics_image.cpp:99`).
   - `ARGB32` is used for `CS_RGB32` (`graphics_cairo.cpp:69-75`, `graphics_image.cpp:98`).
   - Red, blue and green are swapped in `_Stroke`/`_DrawText` (`graphics_cairo.cpp:229,433`).
5. **The branch doesn't build yet** (`libos` CMake target, rich-text doc section 13 item 1). That blocks any HiDPI work on it, so cairo HiDPI is Phase 6, after the main-branch server work, which it reuses.

The main-branch server work (Phase 2) is shared: once `Layer` regions are in device pixels and frames are logical, the `cairo` branch only needs the items above.

## 3. Options considered

| Model | What it means | Verdict |
|---|---|---|
| **A. Server scales logical points (integer s)** | Apps see points. The server keeps regions in device pixels and multiplies at the Layer boundary. Fonts render at `size*s`. 1x bitmaps are doubled. | **Recommended.** Every app works on day one. The work is concentrated in about 6 server files. Same model as macOS and Wayland's `wl_output.scale`. |
| B. Toolkit scales, apps see pixels | Like Windows "system DPI aware": libsyllable multiplies its own metrics by s, fonts get a higher DPI. | Rejected as the main model. Every app with pixel constants (most of them) breaks: big text in small boxes. Kept only as the Phase 1 "large text" stopgap. |
| C. Render at 1x, upscale the whole screen | A half-size back buffer, stretched at flush time. | Rejected. Trivial and compatible, but blurry, with no gain over QEMU's own zoom. |
| D. Fractional (1.25x, 1.5x) | Non-integer s. | Deferred. Rounding of inclusive rects, 1px lines and region seams make it much harder. Do it later as "render at ceil(s) and downscale" (macOS style) or as true fractional through the cairo path. |

## 4. Phased plan

Effort is focused engineering time for one person who knows the codebase, and includes testing in QEMU. Ranges are wide because nothing has been built yet.

### Phase 0: Test setup and plumbing (3 to 5 days)

- Bochs DISPI video driver from the SPICE/QXL plan (2 to 4 days), so QEMU can boot at 2560x1600 and 3840x2160. If it isn't ready, VESA modes that SeaVGABIOS lists can do (verify which ones it lists).
- `ScaleFactor` (int, default 1) in the appserver config, next to `DefaultFont`. Read it once at startup. Live changes come in Phase 5.
- `os::Desktop::GetScaleFactor()` plus `"scale"` in the screen-mode replies (`sapplication.cpp:942,986`) and in `AR_LockDesktopReply_s`. Additive, so old binaries don't care.
- One server helper header, say `scale.h`: `ToDevice(Rect)` with the inclusive-edge rule, `ToDevice(Point)`, `ToLogical(...)`, and `g_nScale`.

### Phase 1 (optional, independent): "Large text" (2 to 3 days)

- Replace the four hard-coded `96`s in `sfont.cpp` with a `FontDPI` setting, then add a slider in the Font or Appearance preferences.
- Useful on its own for accessibility, and on 1080p laptops. It doesn't fix icons, borders or hard-coded widget sizes, and will clip text in some apps. Ship it as "Text size", not "HiDPI".
- Keep it after Phase 2: it combines with scale (for example 2x plus 110% text).

### Phase 2: Server-side integer scale core (4 to 6 weeks)

The heart of the work. All of it is in `appserver/server/`:

1. **Layer geometry** (`layer.cpp`, `layer.h`, `toplayer.cpp`): `SetFrame()` sets `m_cIFrame = ToDevice( cRect )` while `m_cFrame` stays logical. Regions, `ConvertToBitmap()` and the dirty-region logic then work in device pixels unchanged. Check every `GetIFrame()` and `GetIBounds()` caller (36 `IRect`/`IPoint` uses in `layer.cpp`, 17 in `toplayer.cpp`).
2. **Render primitives** (`render.cpp`, 33 uses): `FillRect`, `DrawFrame`, `CopyRect`, `ScrollRect` (scroll delta times s), `Invalidate` from the client, and `DrawBitMap` (the destination goes through `ToDevice`, and the source stays in bitmap pixels, so a 1x bitmap is doubled by the existing scaled-blit path).
3. **Lines:** a 1-point line must be s pixels thick. Horizontal and vertical lines (most of the UI: frames, separators, `DrawFrame`) become `FillRect`. Diagonal lines become s offset Bresenham lines in `ddline.cpp`, which is good enough for UI work. Antialiased lines are the `cairo` branch's job.
4. **Text:** `DrawString`/`DrawText` place the pen at `ToDevice(pen)`. Font instances (`sfont.cpp`) are keyed by (point size, scale) and rasterised at `size * s`. `GetStringWidth`, `GetTextExtent` and `font_height` replies divide by s and return floats. Watch `DrawText()`'s selection handling (`render.cpp:427`: `cSel1`/`cSel2` are `IPoint`s, and I haven't checked whether they hold pixels or character positions).
5. **Screen and windows:** report the logical resolution (pixels / s) everywhere in 1.1. `wndborder.cpp` drag and resize, and `swindow.cpp` frame-changed messages, convert device to logical before sending. Desktop wallpaper is a full-screen bitmap, so load it at device size.
6. **Input:** divide `s_cMousePos` by s when sending `"_scr_pos"` and `"delta_move"` (`input.cpp:108,196`). Keep the internal position in device pixels for the cursor and clamping (`input.cpp:186-193,233`). Hit testing in `layer.cpp`/`swindow.cpp` must compare like with like.
7. **Mouse pointer:** double `g_anMouseImg` and the resize pointers at upload time in `SetCursorBitmap()`, then replace them with real 2x art in Phase 3. Scale the hotspot too.
8. **Decorators:** nothing for `DefaultDecorator` beyond the above. Bitmap decorators get doubled.
9. **Off-screen bitmaps** (`Bitmap` with `ACCEPT_VIEWS`): stay at scale 1 in this phase, so views drawn into bitmaps render at 1x and get doubled on screen. That's correct and compatible, just not sharp. Scale-aware offscreen bitmaps come in Phase 3.

Exit test: every app in the default image at 2x, side by side with 1x, with no seams, clipped text or misplaced clicks. Text and lines are sharp; icons are doubled.

### Phase 3: Sharp bitmaps, icons and pointers (2 to 3 weeks, plus art time)

- `os::Bitmap` gets an optional scale (constructor flag or `SetScale()`). A bitmap of 48x48 pixels at scale 2 has 24x24-point bounds. The server's `DrawBitMap()` already handles the size mismatch. Offscreen views on a scale-2 bitmap render at 2x.
- `BitmapImage::Load()` and resource loading prefer `name@2x.png`, or for icons the next size up (`icon48x48.png` for a 24pt slot at 2x). `IconDirView` and the registrar's `GetTypeAndIcon()` ask for `size * s`.
- `stdbitmaps.cpp` arrows and check marks: redraw them as vectors with `FillRect`/`DrawLine` (they are tiny), or add 2x rasters.
- 96px app icons: Lanczos upscale from 48 as a placeholder, then real art. An SVG icon theme becomes realistic once the SVG translator takes a target size (`svg-support.md` 2.1, item 1; translator `SetConfig()` with width and height, roughly 2 to 3 days).
- 2x mouse pointers and Winter/other decorator pixmaps. This is art, not code.

### Phase 4: Apps and edge cases (2 to 4 weeks, spread out)

- Opt-out flag for apps that need real pixels: `WND_PIXEL_EXACT` or similar. The server treats that window as scale 1 and doubles its whole layer. Use it for `bzoom`, the Camera dock plugin, screen capture (`media/drivers/screen`), SDL, Mesa and video overlays. `Desktop::GetFrameBuffer()` callers must be told the device size and scale.
- Video overlays: the overlay rect goes through `ToDevice`.
- Screenshot (`screenshot.cpp`) captures device pixels. That's correct, so just document it.
- Terminal and CodeEdit: monospace cell metrics become fractional at 2x when divided back. Check that cell widths don't drift; round the cell size in device pixels.
- Go through the 30 `LockRaster()` users and make the ones that matter (dock, Whisper, AView) supply scale-2 bitmaps.
- Webster: stays a 1x app with crisp text until the WebKit upgrade, which then gets `deviceScaleFactor = s`.

### Phase 5: Preferences and live switching (1 week)

- Screen preferences: a "Scale: 100% / 200%" control. Suggest 200% when the mode is wider than about 2400px or EDID reports more than about 180 DPI (EDID comes from the DISPI driver).
- Live change: send `"_new_scale"` alongside `"_new_resolution"` (`swindow.cpp:320-335`) and rebuild font instances and cursors. libsyllable windows already re-layout on resolution change, so check that path rather than writing a new one.

### Phase 6: `cairo` branch HiDPI (1 to 2 weeks after the branch builds)

Items 1 to 4 in section 2: `cairo_scale` in `GraphicsCairo::_BeginUpdate()`, the Pango resolution and font options, `GraphicsImage` at device size, and the three existing bugs. Upgrading cairo to 1.14 or later (device scale on surfaces) is worth considering at the same time: it removes the "reapply scale after `SetTransform`" problem, and the GLib/Pango upgrade work in the rich-text doc already touches the same ports. The upgrade itself is a separate estimate.

### Phase 7 (later): Fractional scaling (3 to 6 weeks)

Two routes. Pick one after Phases 2 to 6 have shipped:

- **Render at ceil(s), downscale on the way to the screen** (macOS). Simple and robust, costs memory and blur, needs a back buffer per screen.
- **True fractional through cairo.** Sharp, but only for `cairo`-path drawing. Legacy `Layer` drawing would need fractional rounding rules everywhere.

## 5. Effort summary

| Phase | Effort | Depends on |
|---|---|---|
| 0. Test setup and plumbing | 3-5 days | (DISPI driver plan) |
| 1. Large text (optional) | 2-3 days | nothing |
| 2. Server integer scale core | 4-6 weeks | 0 |
| 3. Sharp bitmaps, icons, pointers | 2-3 weeks + art | 2 |
| 4. Apps and edge cases | 2-4 weeks | 2 |
| 5. Preferences and live switching | 1 week | 2 |
| 6. `cairo` branch HiDPI | 1-2 weeks | 2, and `libos` building |
| 7. Fractional scaling | 3-6 weeks | 2-6 |
| **Usable 2x desktop (0-3)** | **about 7-11 weeks** | |
| **Through Phase 6** | **about 14-20 weeks** | |

## 6. Risks and open questions

1. **Inclusive rects.** The single biggest source of bugs. Every `IRect( Rect )` in the server must go through `ToDevice()`. A grep for `IRect(` and `IPoint(` in `appserver/server/` (about 290 uses across `layer`, `render`, `sprite`, `wndborder`, `ddriver`, `sapplication`, `swindow`, `toplayer` and `desktop`) is the worklist. Most of them are in driver code, which stays in device pixels.
2. **Off-by-half at fractional logical coordinates.** Apps sometimes draw at `x + 0.5`. With truncation (`rect.h:240`) at 1x that's harmless. At 2x it lands on an odd device pixel. Decide whether `ToDevice()` rounds or floors, and test with TextView and the decorators.
3. **Font metrics divided by 2** become x.5 values. Code that casts `font_height` to `int` (layout, list rows) may lose a pixel per row. Check `ListView`, `TreeView` and `TableView`.
4. **Bitmap decorators and themes** look worse than vector ones at 2x until they get art. That might be the moment to make `DefaultDecorator` (vector) the default.
5. **Not verified:** which large VESA modes SeaVGABIOS lists, how the VMware driver behaves at 2560x1600+, and whether `ddriver_mmx.cpp` blitters assume small sizes. Phase 0 answers these.
6. **No hardware here has been tested.** The plan assumes QEMU first and real HiDPI panels (laptops with eDP at 2560x1600 or more) later, through VESA/GOP or a native driver.
