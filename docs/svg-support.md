# SVG support: what we have, its limits, and the road to full SVG

2026-11-08. Written from `~/Code/Rick-PyroOS/Syllable` (branch `feat/svg`) after the first working build of the SVG translator. Version numbers and dependency floors marked **verify** come from memory, not from the source tarballs; check them against each package's `configure.ac` / `meson.build` before planning work on them.

## 1. What we have today

| Piece | Where | Notes |
|---|---|---|
| librsvg 2.22.3 (2008) | `cmake/ports/30-cairo.cmake`, `Builder/packages/librsvg-2.22.3` | Built from the 2008 Syllable port (`Syllable-Ports/archives/librsvg-2.22.3.zip`, mounted by `sb` at `/ports-archives`). Library only, with its own Makefile: upstream `configure` requires gdk-pixbuf. Installed in `/usr/librsvg/2.22.3`. |
| GdkPixbuf stand-in | `Builder/packages/librsvg-2.22.3/patches/gdk-pixbuf.[ch]` | A small GObject holding an RGB(A) buffer, with the accessors librsvg uses. Enough for filters and `rsvg_handle_get_pixbuf()`. It has no image loaders. |
| SVG translator | `system/sys/translators/svg/svgtrans.cpp` | Reads the whole document, has librsvg draw it with cairo into a buffer, un-premultiplies the alpha, and emits one `CS_RGB32` frame. Identify looks for the opening `svg` tag in the first 8 KB. Links libpthread. |
| `BitmapImage::Load()` fix | `system/sys/appserver/libsyllable/gui/image.cpp` | Sends the final `AddData()` call even when the file size is an exact multiple of 8 KB (the translator draws only on that call). |
| File type | AView (`main.cpp`, `MSG_MIMETYPE_IMAGE_SVG` in all 13 catalogs), `system/image/registrar-types.json`, `system/other/icons/filetypes/image_svg.png` | `image/svg+xml`, extension `svg`, handled by AView. |
| Test files | `system/image/home/root/Desktop/SVG-Examples/` | Jonas' svgviewer examples, `transparency.svg`, `exactly-16384-bytes.svg`. |

Dependencies, all from `30-cairo.cmake` and built with GCC 9.5.0: GLib 2.14.4 (no threads), cairo 1.4.14, Pango 1.20.2, FreeType 2.3.9, fontconfig 2.5.0, libxml2 2.7.8 (shared).

## 2. Limits

### 2.1 Limits of rendering to a bitmap

The translator API turns a file into pixels once. That decides several things:

1. **Fixed size.** The SVG is drawn at its declared width and height. Zooming in AView, larger icon sizes, or a HiDPI screen stretch those pixels and blur. The translator API carries no target size. `Translator::SetConfig()` could take one, but `BitmapImage::Load()` never calls it.
2. **No size cap.** The translator allocates width × height × 4 bytes for whatever the file declares, so a file claiming 100000×100000 asks for about 40 GB. **Fix:** cap at, for example, 4096 pixels on the longer side and scale with `cairo_scale()` to fit.
3. **Files without a size.** If a document has only a `viewBox`, or uses percentage sizes, librsvg 2.22 invents a default (often 100×100). **Fix:** when there is a `viewBox` but no width or height, use the `viewBox` size.
4. **Whole file in memory, drawn on the last chunk.** Fine for SVG (it can't be drawn progressively anyway), but large documents show nothing until fully read.

### 2.2 Limits of librsvg 2.22

1. **No embedded raster images.** `<image>` elements are skipped, because the stand-in has no `GdkPixbufLoader`. **Fix without upgrading:** implement `gdk_pixbuf_loader_*` in the stand-in on top of Syllable's own translators (PNG, JPEG, GIF...), and un-comment `rsvg_pixbuf_new_from_href()` in `rsvg-image.c`.
2. **No `.svgz`.** Gzipped SVG needs libgsf in 2.22 (`--with-svgz`). **Fix without upgrading:** gunzip in the translator with zlib before handing the data to librsvg, and detect the gzip magic in Identify.
3. **Basic CSS.** Without libcroco, only `style` attributes and simple selectors work. Stylesheets in `<style>` with real selectors, which most Inkscape and Illustrator output relies on, are partly ignored. See Section 4.
4. **Old rendering bugs.** Fifteen years of fixes to text layout, markers, masks, filters and gradients are missing.
5. **No animation and no scripting.** No version of librsvg supports either. See Section 5.
6. **Security.** 2.22 has known parser bugs. It is fine for local files; don't point it at untrusted downloads without upgrading.

## 3. Upgrading librsvg

### 3.1 Which version

- **2.40.x is the last C release series** (the last one is 2.40.21, **verify**). It is the realistic target.
- **2.41 and later are Rust** (the port to Rust started in 2.41 and finished by 2.45, **verify**). Building them needs a Rust compiler targeting `i586-pc-syllable`, which doesn't exist: it would mean a Syllable target in rustc and a Rust standard library port. Out of scope for now. If Rust support ever comes, it would be its own project, and resvg (Section 5.3) becomes an option too.

### 3.2 What 2.40 needs (dependency floors, **verify** against `configure.ac`)

| Dependency | 2.40 needs (approx.) | We have | Gap |
|---|---|---|---|
| GLib | ≥ 2.12, GIO | 2.14.4 | 2.40's GIO use (GFile, GInputStream) needs a newer GLib. 2.30.3 is in `Syllable-Ports/archives`. GLib ≥ 2.29 needs libffi, a new port. |
| gdk-pixbuf | ≥ 2.20, real (not optional) | stand-in | Port gdk-pixbuf 2.24.1 (in `Syllable-Ports/archives`). It needs GLib ≥ 2.25 (**verify**), so it comes after the GLib upgrade. Build its PNG and JPEG loaders built-in (`--with-included-loaders`), since GModule loading is off. |
| libcroco | ≥ 0.6.1, **required** in 2.40 | none | New port, Section 4. |
| cairo | ≥ 1.2 (newer for some features) | 1.4.14 | Probably fine; check `configure.ac`. |
| Pango | ≥ 1.38 (**verify**) | 1.20.2 | Pango 1.29.4 is in `Syllable-Ports/archives`; 1.38+ needs HarfBuzz (another port). This is likely the biggest chain. |
| libxml2 | ≥ 2.7 | 2.7.8 shared | Fine. |

### 3.3 Order

1. libffi port, then GLib 2.30.3 (shared, SYSROOT) in place of 2.14.4. Rebuild Pango and anything else that links GLib.
2. gdk-pixbuf 2.24.1 with built-in loaders.
3. libcroco (Section 4).
4. HarfBuzz and a newer Pango, if 2.40 really needs Pango ≥ 1.38 (**verify** first; if an older 2.3x librsvg works with Pango 1.29, stopping there is a reasonable midpoint).
5. librsvg 2.40.x with its own `configure` (no more custom Makefile or stand-in). Its public API is the same `rsvg_handle_*` plus `rsvg_handle_render_cairo()`, so the translator should need no changes beyond dropping `rsvg_init()` (deprecated) and possibly `g_type_init()`.
6. Retire `Builder/packages/librsvg-2.22.3` and the stand-in.

Each step is a `syllable_port()` in `30-cairo.cmake`, built with GCC 9.5.0 like the rest of the cairo stack.

## 4. Porting libcroco

libcroco is GNOME's CSS2 parser and selection engine. librsvg 2.22 can use it optionally (`--with-croco`), and 2.40 requires it.

- **Version:** 0.6.13 is the last release (2019). The project is archived and unmaintained, and has open CVEs in its parser. That's acceptable for local files, but it's a reason not to treat it as long-term.
- **Dependencies:** GLib ≥ 2.0 and libxml2 ≥ 2.4 (**verify**). Both are already ported, so it can be built today against GLib 2.14 and libxml2 2.7.8.
- **Port shape:** a plain autotools `syllable_port(libcroco ... SHARED SYSROOT DEPENDS glib libxml2_shared)` in `30-cairo.cmake`, prefix `/usr/libcroco/0.6.x`. Nothing Syllable-specific is expected.
- **Use it now, with 2.22:** add `-DHAVE_LIBCROCO`, libcroco's pkg-config flags, and `rsvg-css.c`'s croco path to the port Makefile. That gives real `<style>` sheets without waiting for the 2.40 upgrade. This is the cheapest visible improvement in this whole document.

## 5. Animation and scripting

### 5.1 What they are

- **Animation** in SVG is SMIL: `<animate>`, `<animateTransform>`, `<animateMotion>`, `<set>`, with timing (`begin`, `dur`, `repeatCount`) and events. Many modern "animated SVGs" use CSS animations instead (`@keyframes` in `<style>`).
- **Scripting** is `<script>` with JavaScript and the SVG DOM. Event handlers like `onclick` change the document at run time.

### 5.2 Why librsvg can't do them

librsvg is a renderer: it loads a document and draws one still frame. No version implements SMIL, CSS animations, or scripts, and it has no DOM API that would let us change the document between frames. Building animation on top of it would mean writing our own SVG DOM and re-serialising the document for every frame. That isn't worth it.

### 5.3 Options

1. **WebKit (recommended).** WebCore implements SVG with SMIL, CSS animations, and scripting through JavaScriptCore. Webster's 2008 WebKit (r29946) already has `ENABLE_SVG`, `ENABLE_SVG_FONTS` and `ENABLE_SVG_AS_IMAGE`. `ENABLE_SVG_ANIMATION` exists in `svgtags.in` but is not in Webster's feature list (**verify** whether it builds). The real path is the WebKit upgrade in `docs/webster-webkit-upgrade.md` (WebKitGTK 2.38–2.40 line), whose SVG support is complete. Animated or scripted SVGs would then open in Webster, and an "SVG view" could embed a WebView.
2. **A small SMIL player on our own SVG DOM.** Only if we need animated SVG outside a browser (animated icons, for example). It needs a DOM, a timing model and an interpolation engine, and it would render each frame with cairo. That's a large job; consider it only after WebKit.
3. **resvg / usvg (Rust).** Faster and more correct than librsvg, but static-only like librsvg, and blocked on Rust like librsvg 2.41+.

### 5.4 What the translator should do with animated files

Draw the first frame (the document's state before any animation), which is what it does today. Scripts are never run. That is the safe behaviour for an image translator and should stay that way, even after a WebKit upgrade.

## 6. Scalable SVG in the GUI (beyond the translator)

To draw SVGs sharply at any size (icons, AView zoom, HiDPI), add an `os::Image` subclass, for example `SVGImage` in libsyllable:

- holds the `RsvgHandle` (parsed once);
- `SetSize()` / `GetSize()` re-render into an internal `Bitmap` at the requested size, with `cairo_scale()` from the document's size;
- `Draw()` blits that bitmap, as `BitmapImage` does;
- on the **cairo branch**, `Draw()` can call `rsvg_handle_render_cairo()` on the view's cairo context directly, with no bitmap in between;
- icon code (Desktop, file requesters) and AView create an `SVGImage` for `image/svg+xml` instead of going through the translator, and ask for the size they need.

The translator stays for generic code paths that only know `BitmapImage`.

## 7. Suggested order

| Step | Work | Size | Depends on |
|---|---|---|---|
| 1 | Translator: size cap and `viewBox` fallback (2.1.2, 2.1.3) | small | — |
| 2 | libcroco port, enabled in librsvg 2.22 (Section 4) | small | — |
| 3 | `.svgz` through zlib in the translator (2.2.2) | small | — |
| 4 | Stand-in `GdkPixbufLoader` over Syllable translators (2.2.1) | medium | — |
| 5 | `SVGImage` class, used by AView zoom and icons (Section 6) | medium | — |
| 6 | libffi, GLib 2.30, gdk-pixbuf 2.24 | large | — |
| 7 | HarfBuzz and newer Pango, if needed | large | 6 |
| 8 | librsvg 2.40.x, retire the 2.22 port | medium | 2, 6, 7 |
| 9 | Animation and scripting through WebKit | large | `docs/webster-webkit-upgrade.md` |
