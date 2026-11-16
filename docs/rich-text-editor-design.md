# Rich text editor on the `cairo` branch: design and implementation plan

2026-11-08. Written from a read of the `cairo` branch (83e0a251, last changed 2026-04-26) and `main` (8ebddd07) with git, without checking either out or building them. Line numbers refer to those commits.

## 0. Summary

The plan is a reusable widget, `os::RichTextView`, in a new library (`librichtext`) built on the `cairo` branch's `libos`, plus a small word-processor app on top of it (working name **Scribe**).

The main design decision is **where text gets laid out and drawn**. The `cairo` branch has two text paths:

1. **Server side.** `GraphicsView::DrawTextLayout()` sends the text or Pango markup to the appserver in a `GFX_DRAW_TEXTLAYOUT` packet. The server builds a fresh `PangoLayout` from it and draws it with cairo.
2. **Client side.** `GraphicsImage` (a `GraphicsBackend`) wraps an `os::Bitmap` in a cairo image surface inside the app, and `DrawTextLayout()` calls `pango_cairo_show_layout()` on the app's own `PangoLayout`.

The editor should use **path 2**. It lays out each paragraph with Pango in the app, uses those same layouts for hit testing, caret and selection, renders them with cairo into an RGB32 back buffer, and blits that with the old `View::DrawBitmap()`. Section 1.3 has the reasons. In short, path 1 can't carry attribute lists, caps a draw at 8180 bytes, lays the text out twice with two different Pango contexts, and drops everything unless the screen is 32-bit. Path 2 has none of those problems, and it doesn't depend on the unfinished server half of the branch.

Don't build this by extending `TextView`. Its buffer is `std::vector<String>` (one string per line), it measures with `Font::GetStringWidth()`, and its back buffer is `CS_RGB16`. A rich editor needs styled paragraphs, Pango measurement and a 32-bit surface, so almost nothing carries over except the event-mask conventions, which the new widget copies.

Files go through Syllable's translator framework, with a new rich text intermediate type alongside the image one, so HTML, Markdown, RTF and plain text are add-ons and more formats can be added without touching the app (Section 9). Markdown gets a translator built on cmark-gfm, typing shortcuts and a source view (Section 9.6). Code blocks are highlighted with CodeView's existing language formats, moved into a small shared library (Section 10).

The model (document, positions, style runs, undo, the rich text stream) should have no `os::` dependencies, so it can be built and unit-tested on the Mac in seconds rather than in the 1000 MHz QEMU guest.

## 1. What the `cairo` branch gives a text editor

### 1.1 The pieces that matter

| Piece | Where (on `cairo`) | Use for the editor |
|---|---|---|
| `graphics::TextLayout` | `libos/graphics/textlayout.cpp`, header `include/pyro/pyro/graphics/textlayout.h` | Wraps a `PangoLayout`. `GetPangoLayout()` returns it, so the editor can set attribute lists and query positions directly. |
| `graphics::GFont` | `libos/graphics/gfont.cpp` | Wraps `PangoFontDescription` (family, style, weight, stretch, size). Good for the font UI and for named styles. |
| `graphics::GraphicsImage` | `libos/graphics/graphics_image.cpp` | Cairo on an `os::Bitmap` in the app process. This is the render target. |
| `graphics::GraphicsBackend::DrawTextLayout()` | `graphics_backend.cpp:429-436` | Draws the app's own `PangoLayout`, with all attributes. |
| `graphics::GraphicsPS` | `graphics_ps.cpp` | PostScript output. The cairo port is also built with `--enable-pdf` (`cmake/ports/30-cairo.cmake`), so PDF export is the same code with `cairo_pdf_surface_create()`. |
| `View::Paint( const Rect&, graphics::Graphics& )` | `libos/gui/view.cpp:2060`, called from `window.cpp:1606-1612` | The new paint hook. The editor doesn't need it (Section 5.3). |

Library versions, from `cmake/ports/30-cairo.cmake`: cairo 1.4.14, Pango 1.20.2 (built-in shaping modules, no HarfBuzz), GLib 2.14.4 built `--with-threads=none`, fontconfig 2.5.0, FreeType 2.3.9, libxml2 2.7.8 (shared).

### 1.2 Pango 1.20 limits to design around

Everything an editor needs for layout and hit testing is in 1.20: `pango_layout_set_attributes()`, `pango_layout_xy_to_index()`, `pango_layout_index_to_pos()`, `pango_layout_get_cursor_pos()`, `pango_layout_move_cursor_visually()`, `pango_layout_get_log_attrs()`, `PangoLayoutIter`, `pango_layout_line_get_x_ranges()`, `pango_layout_set_tabs()`, `PangoAttrShape` and `pango_cairo_context_set_shape_renderer()` (1.18).

Not available, so don't write code that assumes them:
- `pango_layout_get_baseline()` (1.22). Use `pango_layout_iter_get_baseline()` on a fresh iterator.
- `pango_layout_get_caret_pos()` (1.50) for slanted carets. Draw an upright caret.
- Font features (`pango_attr_font_features_new`, 1.38), automatic hyphenation (1.44), `pango_attr_insert_hyphens`. OpenType features need HarfBuzz and a newer Pango, the same chain `docs/svg-support.md` describes for librsvg.
- `cairo_format_stride_for_width()` (cairo 1.6). For 32-bit formats `GetBytesPerRow()` is always a multiple of 4, which is all cairo 1.4 needs.

### 1.3 Why not the server-side text path

Reading `graphics_view.cpp:46-83` and `graphics_cairo.cpp:381-441`:

- **No attributes.** The packet carries the font description string, the text (plain, markup or markup with accelerator) and layout flags. A `PangoAttrList` set on the client layout never reaches the server. The only way to send styling is to serialise it to Pango markup, and markup can't express everything the editor needs (shape attributes for images, for example).
- **Size cap.** It goes in a normal render packet, and `Window::_AllocRenderCmd()` refuses anything over `RENDER_BUFFER_SIZE` (8180 bytes, `protocol.h:704`). One long paragraph of markup won't fit.
- **Two layouts.** The client measures with a `PangoContext` from `pango_cairo_font_map_create_context()` with no `pango_cairo_update_context()` and default font options (`textlayout.cpp:45-53`). The server lays the text out again with its own cached context (`graphics_cairo.cpp:517-527`). If resolution or hinting differ, the caret the client computes won't sit where the server drew the glyph.
- **Relayout every paint.** The server builds and frees a `PangoLayout` on every draw.
- **32-bit framebuffer only.** `_BeginUpdate()` drops every `GFX_*` command unless the target bitmap is RGB32/RGBA32 (appserver review 7.1). The default image is RGB16.
- **Wrong colour.** `_DrawText()` calls `cairo_set_source_rgba( psCairo, nRed, nBlue, nGreen, nAlpha )` (`:433`, same in `_Stroke()` at `:229`), so green and blue are swapped for all server-side text and strokes.

Path 2 avoids all of this. The app draws into an `os::Bitmap` it shares with the server (`SHARE_FRAMEBUFFER`), and the appserver blits and converts it with the old `DrawBitmap()`, which works on any screen depth. The server-side path is still fine for labels and buttons, and the colour swap should be fixed for them anyway.

### 1.4 What still has to happen on the branch

The branch doesn't build yet (appserver review 7.1: Makefile-only, commit c61b1651 says it doesn't compile, `theme.cpp` is broken). The editor needs only part of it working:

| Needed for the editor | Not needed for the editor |
|---|---|
| `libos` compiles and links cairo, pango, glib and fontconfig (its `Makefile:59` already lists them) | The server `GFX_*` path, `graphics_cairo.cpp`, the region and hidden-layer bugs |
| `graphics/` (`TextLayout`, `GFont`, `GraphicsBackend`, `GraphicsImage`) compiles | A 32-bit framebuffer |
| A CMake target for `libos` so `librichtext` can link it | Themes (`graphics::theme`) |
| The fixes in Section 13 for `TextLayout` and `GraphicsImage` | Moving the window system or decorators to cairo |

An app links either `libos` or `libsyllable`, not both, because both define the `os::` classes. So Scribe is a `libos` app.

## 2. Architecture

```
  Scribe (app)            menus, toolbar, file I/O, find/replace, PDF export
      |
  os::RichTextView        Control: scrollbars, invoker messages, public API      librichtext
      |
  RichTextEdit (View)     input, caret, selection, painting, autoscroll
      |
  LayoutCache             one PangoLayout per paragraph, y offsets, heights
      |
  rt::Document            paragraphs, style runs, named styles, undo        (no os:: deps)
      |
  rt::Read/WriteStream    the rich text stream (Section 9.2)                  (no os:: deps)
      |
  translators (.so)       html, markdown, rtf, plaintext  <->  files, clipboard

  libsyntax               CodeView's language formats, used by LayoutCache for code blocks
```

Same split as `TextView`/`TextEdit`: the outer control owns the scrollbars and the public API, and the inner view does the work. The model sits underneath in namespace `rt`, written against `std::string` and plain structs so it compiles on the host.

Source layout:

```
system/libs/richtext/
  CMakeLists.txt
  model/      document.{h,cpp} position.h charstyle.{h,cpp} parastyle.{h,cpp}
              stylesheet.{h,cpp} undo.{h,cpp} stream.cpp
  layout/     layoutcache.{h,cpp} attrbuild.cpp shapes.cpp textlock.{h,cpp} syntax.cpp
  view/       richtextview.cpp richtextedit.{h,cpp} caret.cpp selection.cpp keymap.cpp
              inputrules.cpp (Markdown shortcuts)
  tests/      host unit tests for model/ and layout/
system/sys/include/pyro/pyro/gui/richtextview.h     public header
system/apps/utils/Scribe/                           the app
system/libs/syntax/                                 libsyntax: format.h, format_*.cpp from libcodeview, registry
system/sys/translators/text/{plaintext,html,markdown,rtf}/
system/sys/include/pyro/pyro/translation/richtextstream.h
```

`system/libs/codeview` is the precedent for a widget library outside `libos`.

## 3. Document model

### 3.1 Structure

A document is a vector of paragraphs. A paragraph is the unit Pango lays out, so this maps one to one onto `PangoLayout`s.

```cpp
namespace rt {

struct Position {            // a caret position
    int para;                // paragraph index
    int offset;              // UTF-8 byte offset in the paragraph (Pango indices are bytes)
};

struct Range { Position start, end; };   // start <= end after Normalise()

struct Run {                 // character formatting over [start, end) in one paragraph
    int start, end;          // byte offsets
    StyleRef style;          // interned CharStyle (flyweight, shared, compared by pointer)
};

struct Paragraph {
    std::string text;        // UTF-8, no '\n'. U+FFFC marks an inline object.
    std::vector<Run> runs;   // sorted, non-overlapping, cover [0, text.size()), merged when equal
    ParaStyle para;          // alignment, indents, spacing, list, named style
    std::vector<InlineObject> objects;   // images, keyed by byte offset of their U+FFFC
    uint32_t revision;       // bumped on any change, for the layout cache
};

class Document {
public:
    // Editing primitives. Everything else (typing, paste, style toggles) is built from these,
    // and each one returns the undo command that reverses it.
    std::unique_ptr<Command> Insert( Position pos, const Fragment& frag );
    std::unique_ptr<Command> Delete( Range r );
    std::unique_ptr<Command> ApplyCharStyle( Range r, const CharStyleDelta& delta );
    std::unique_ptr<Command> ApplyParaStyle( int firstPara, int lastPara, const ParaStyleDelta& delta );

    Fragment Copy( Range r ) const;          // styled slice, for clipboard and undo
    CharStyle StyleAt( Position pos ) const; // effective style (sheet + paragraph style + run)

    // Navigation that needs only text (no layout)
    Position NextGrapheme( Position ) const; // uses g_utf8_next_char; fine for v1
    Position PrevGrapheme( Position ) const;

    void AddObserver( DocumentObserver* );   // ParagraphsChanged( first, last, delta )
private:
    std::vector<Paragraph> m_paras;
    StyleSheet m_sheet;
};

}
```

Why these choices:

- **Byte offsets.** Every Pango position API takes and returns byte indices into the layout text, so storing anything else means converting on every caret move.
- **Paragraph vector, not a rope or piece table.** Edits touch one or two paragraphs, and inserting into a vector of a few thousand paragraphs is cheap. A long novel is around 5,000 paragraphs. If that ever becomes slow, the vector can turn into a balanced tree behind the same `Document` API.
- **Runs rather than per-character styles.** Runs map straight onto `PangoAttribute` start and end indices. Keep them merged so equal neighbours become one run.
- **Interned styles.** `CharStyle` values come from a `StyleTable` that returns one shared instance per distinct value. Comparing runs is then a pointer compare, and a document with 10,000 bold runs holds one bold style.

### 3.2 Styles

```cpp
struct CharStyle {           // every field optional: "unset" means inherit
    opt<std::string> family;
    opt<double>      sizePt;
    opt<int>         weight;        // 100..900, maps to PangoWeight
    opt<bool>        italic;
    opt<int>         underline;     // none, single, double, error (PangoUnderline)
    opt<bool>        strikethrough;
    opt<Rgba>        fg, bg;
    opt<int>         baseline;      // normal, superscript, subscript (rise + scale)
    opt<std::string> link;          // href; drawn underlined, opened by the app
    opt<bool>        code;          // inline code: monospace + background; writers emit `code` / <code>
};

struct ParaStyle {
    std::string named;       // "Body", "Heading 1", "Quote", "Code" ...
    opt<int>    align;       // left, centre, right, justify
    opt<double> indentLeft, indentRight, indentFirst;   // points
    opt<double> spaceBefore, spaceAfter;                // points
    opt<double> lineSpacing;                            // extra points between lines
    opt<int>    listType;    // none, bullet, number, checkbox
    opt<int>    listLevel;
    opt<int>    listStart;   // first number of a numbered list
    opt<bool>   checked;     // checkbox list items
    opt<std::string> codeLanguage;  // Code paragraphs: "cpp", "markdown" ... (Section 10)
    std::vector<double> tabs;
};
```

The effective style of a character is: stylesheet default, then the named paragraph style, then the run's direct formatting. That's the usual word-processor rule, and it means changing "Heading 1" in the stylesheet restyles every heading.

The stylesheet ships with Body, Heading 1-6, Quote, Code (monospace) and List. Six headings because Markdown and HTML have six. Users don't edit named styles in v1, and the writers map them to tags and Markdown syntax (Section 9).

Two kinds of inline object exist: images and horizontal rules. A line break inside a paragraph (Shift+Enter, or a Markdown hard break) is U+2028, which Pango breaks the line at.

### 3.3 Insertion style

The caret has its own "typing style". It starts as the style of the character before the caret (or after it at the start of a paragraph), and toggling Bold with no selection changes only the typing style. The next insert uses it. Any caret move resets it from the text. Every editor works this way, and without it Ctrl+B with no selection appears to do nothing.

### 3.4 Undo

Command objects, built only from the four primitives:

```cpp
struct Command {
    virtual void Undo( Document& ) = 0;
    virtual void Redo( Document& ) = 0;
    virtual bool Merge( const Command& next ) { return false; }   // typing coalescing
    Range selectionBefore, selectionAfter;                          // restored on undo/redo
};
```

- **Grouping.** One user action can be several primitives (paste over a selection is Delete then Insert). `UndoStack::BeginGroup()`/`EndGroup()` wraps them in one entry.
- **Coalescing.** Consecutive single-character inserts in the same paragraph, with no caret move between them and less than about a second apart, merge into one entry. Break at whitespace after a word, as most editors do.
- **Limit.** Keep `SetMaxUndoSize()` from `TextView`, default 1000 entries.
- **Saved state.** Record the undo depth at the last save, so the app can show "modified" correctly after undoing back to it.

## 4. Layout

### 4.1 One `PangoLayout` per paragraph

`LayoutCache` holds, for each paragraph, a `PangoLayout*`, the paragraph's `revision` when it was built, its height, and the paragraph's y offset in the document.

Building a layout (`attrbuild.cpp`):

1. `pango_layout_set_text()` with the paragraph text.
2. Width in Pango units: view width minus page margins minus `indentLeft`/`indentRight`. `pango_layout_set_wrap( PANGO_WRAP_WORD_CHAR )`.
3. `pango_layout_set_indent()` for the first-line indent, `set_alignment()`, `set_justify()`, `set_spacing()`, and `set_tabs()` from `ParaStyle`.
4. A `PangoAttrList` with one font description attribute for the paragraph's base style and one attribute per run for each field the run overrides: `pango_attr_family_new`, `size_new`, `weight_new`, `style_new`, `underline_new`, `strikethrough_new`, `foreground_new`, `background_new`, `rise_new` + `scale_new` for super and subscript.
5. One `pango_attr_shape_new_with_data()` per inline object, covering its U+FFFC, with the image size as the logical rectangle.
6. `pango_layout_set_auto_dir( TRUE )` so right-to-left paragraphs work.

`spaceBefore` and `spaceAfter` aren't Pango concepts. The cache adds them to the paragraph's height and offsets the drawing.

List bullets and numbers are drawn by the editor in the left indent, not inserted into the text, so they never get selected or copied as characters.

### 4.2 Y offsets and lazy layout

The cache keeps `y[i]`, the top of paragraph `i`, as a prefix sum, plus a "valid up to" index. Editing paragraph `k` invalidates `y` from `k+1` on, which costs nothing until someone asks. Finding the paragraph at a given y is a binary search.

When the width changes (window resize), every layout is stale. Rebuild the visible ones at once, then lay out the rest in batches of about 50 paragraphs from a `Looper` timer, so the window stays responsive. Until a paragraph has been laid out, estimate its height from its byte length and the average line height, and correct the scrollbar as real heights arrive. The old `TextView` gets away without this because every line has the same height.

### 4.3 Pango context

All layouts share one `PangoContext` owned by the cache:

- created with `pango_cairo_font_map_create_context()`;
- `pango_cairo_context_set_resolution( ctx, 96 )`, or whatever the appserver's DPI setting is, so that "12 pt" means the same on screen in every app;
- `pango_cairo_context_set_font_options()` with antialias and hinting set once, so measurement matches drawing;
- `pango_cairo_context_set_shape_renderer()` for inline images (Section 5.4).

The PDF exporter makes a second context at 72 DPI and builds its own layouts, because page width and resolution differ from the screen.

### 4.4 Threading

This is the easiest thing to get wrong. GLib is built with `--with-threads=none`, and Pango 1.20's `pango_cairo_font_map_get_default()` returns one process-wide font map, which isn't thread-safe. Fontconfig 2.5 isn't thread-safe either. Every `os::Window` runs its own looper thread, so two Scribe windows would call Pango from two threads at once.

So every Pango, pangocairo and fontconfig call in the process goes through one lock: `graphics::TextLock()`, a process-wide `os::Locker`, taken by `LayoutCache` around building and querying layouts and by the renderer around drawing them. Put it in `libos`'s `graphics/` (Section 13), because `TextLayout` and `GraphicsBackend` need the same lock for any other widget that uses them. Using a recursive lock avoids deadlocks when a widget calls into `TextLayout` while it holds it. Cairo 1.4 has its own mutexes if it found pthreads at configure time. That should be checked in the build log, but the lock covers cairo calls made on Pango's behalf either way.

### 4.5 GLib thread support

Turning GLib's threads on is a small job, but it doesn't remove the need for `TextLock()`.

**What it takes with GLib 2.14:**

1. **Rebuild GLib with `--with-threads=posix`** instead of `--with-threads=none` (`cmake/ports/30-cairo.cmake:98`). Because it's a cross-compile, `cmake/ports/glib.cache` will probably need cached answers for the thread checks configure can't run, for example `glib_cv_sizeof_system_thread=4` and the thread priority range (`POSIX_MIN_PRIORITY`/`POSIX_MAX_PRIORITY`, or `sched_get_priority_min/max`).
2. **libpthread already has what `gthread-posix.c` uses:** create, join and detach, mutexes, condition variables, keys and `pthread_once` (`docs/posix-audit-2026-10-30.md`, Threads). There are two weak spots. `sched_yield` is an ENOSYS stub, which only affects `g_thread_yield()`. Every `pthread_cond_timedwait` starts a kernel thread as its timer (`libpthread/cond.c:202`), so anything that does timed waits in a loop, such as `g_async_queue_timed_pop()` or `g_cond_timed_wait()`, gets expensive. Fix that in libpthread before relying on timed waits.
3. **Call `g_thread_init( NULL )` first.** Before GLib 2.32 it's required, and it has to come before any other GLib call in the process. Put it in `libos`'s `Application` constructor, behind a once guard, and at appserver startup if the server's Pango path stays.
4. **Rebuild Pango, cairo and librsvg** against the new GLib, then test with two windows doing text work at the same time and a GLib main loop in a second thread.

That's about a day of work if configure cooperates.

**Why `TextLock()` stays:** GLib threads only make GLib's own data structures thread-safe (`GHashTable` locking, the type system, `g_once`). They don't change the libraries above it:

- Pango 1.20 still has one shared default font map, and its shaping and font caches aren't locked. Per-thread default font maps and real thread safety came with Pango 1.32, and Pango 1.31 onwards requires HarfBuzz.
- Fontconfig 2.5 has no locking at all. It became thread-safe in 2.10.91/2.11.
- Those newer versions need GLib 2.30 or later, which needs libffi. That's the same upgrade chain `docs/svg-support.md` lays out for librsvg (libffi, GLib 2.30, HarfBuzz, newer Pango).

**Recommendation:** leave GLib threads off for now and keep the single text lock. Turn them on when something needs GLib's own threading, such as GIO async I/O or a newer librsvg, and do it as part of the GLib 2.30 upgrade rather than on 2.14. After that upgrade, with Pango 1.32 or later and fontconfig 2.11 or later, `TextLock()` could shrink to guarding only the shared `PangoContext` and the `LayoutCache`, and each window could lay out text independently.

## 5. Rendering

### 5.1 The back buffer

`RichTextEdit` owns one `os::Bitmap` the size of its visible area, `CS_RGB32` with `SHARE_FRAMEBUFFER`, and a `graphics::GraphicsImage` (or a raw `cairo_t`) on it. `Paint( cUpdateRect )` does:

1. Under `TextLock`, for each paragraph that intersects the update rectangle (binary search on `y`):
   - fill the paragraph's band with the page background;
   - draw selection highlight rectangles: for each line, `pango_layout_line_get_x_ranges()` over the selected byte range, then `FillRectangle()`;
   - draw list markers in the indent;
   - `cairo_move_to()` to the paragraph origin and `pango_cairo_show_layout()`.
2. `DrawBitmap( pcBuffer, cUpdateRect, cUpdateRect )` with the old `View` API.
3. Draw the caret with the old API (Section 6.2).

The back buffer is opaque, so cairo's premultiplied alpha isn't an issue. Recreate it only when the view grows past it or shrinks to under half of it, as `TextEdit::UpdateBackBuffer()` already does.

### 5.2 Damage and scrolling

- **Typing in one paragraph** invalidates that paragraph's band. If its height changed, everything below it in view is invalidated too.
- **Scrolling** uses `ScrollBy()`, which makes the server copy the visible pixels, so only the exposed strip is repainted. In the back buffer, `memmove()` the raster rows by the same amount so the buffer stays in step with the screen, then render the strip.
- **Selection change** invalidates the union of old and new selection bands only.

### 5.3 Why not paint through `View::Paint( Rect, Graphics& )`

On the branch, `Window` calls the old `Paint()` and then the new one with a fresh `GraphicsView` on every `M_PAINT` (`window.cpp:1606-1612`). That second path is the server-side one from Section 1.3. The editor overrides only the old `Paint()` and leaves the new one empty, so it costs one empty virtual call. If the server path later gets a "draw this client bitmap" fast path or proper attribute support, the renderer can move without touching the model or the input code.

### 5.4 Inline images

An image is a U+FFFC in the text, an `InlineObject` holding a decoded `os::Bitmap` (from the translator framework), and a shape attribute with its size. The shape renderer callback gets the cairo context and the attribute's data pointer, and paints the bitmap with `cairo_set_source_surface()` on an image surface over the bitmap's raster. Images are selected, copied and deleted like one character.

## 6. Editing

### 6.1 Hit testing and caret movement

All positions come from the layout, never from summing glyph widths as `TextEdit::GetPixelPosX()` does:

| Action | Pango call |
|---|---|
| Click → position | binary-search the paragraph by y, then `pango_layout_xy_to_index()`; add `trailing` graphemes to the index |
| Position → caret rect | `pango_layout_get_cursor_pos()` (strong cursor; the weak one is the second caret in mixed-direction text) |
| Left / Right | `pango_layout_move_cursor_visually()`; it handles graphemes and bidi. Crossing the start or end moves to the neighbouring paragraph. |
| Ctrl+Left / Right | `pango_layout_get_log_attrs()`, look for `is_word_start` / `is_word_end` |
| Up / Down | keep a "goal x" from the last horizontal move; find the line above or below with `PangoLayoutIter`, then `pango_layout_line_x_to_index()` at goal x. Crossing a paragraph boundary uses the next paragraph's first or last line. |
| Home / End | start or end index of the current `PangoLayoutLine` |
| Ctrl+Home / End | document start or end |
| Page Up / Down | move goal y by the view height, then hit-test |
| Double click | word under the pointer, from log attrs |
| Triple click | whole paragraph |
| Backspace / Delete | previous or next cursor position (`is_cursor_position`), so a combining sequence is deleted whole |

### 6.2 Caret

The caret is a 1-pixel bar (2 at large sizes), the height of the line at the caret, from `get_cursor_pos()`. It isn't drawn into the back buffer. After the blit, draw it with `FillRect()` in the old API. To blink it off, blit that small rectangle from the back buffer again. A blink then costs one tiny blit and no Pango work. Blink with a `Looper` timer, about 530 ms, and reset it on every key press.

### 6.3 Keyboard

`KeyDown( pzString, pzRawString, nQualifiers )` gives UTF-8 in `pzString`, and dead keys are already composed by the keymap, so a printable string is inserted as is, with the typing style. Special keys come through `pzString[0]` as `VK_*` values, as in `TextEdit::HandleKeyDown()`.

The widget handles editing and navigation keys, Shift-extended selection, Ctrl+A/C/X/V/Z/Y, and Ctrl+B/I/U for the typing style or selection. Everything else (save, open, find) is the app's, through menu shortcuts. `SetReadOnly()` turns off everything that edits but keeps navigation and copy.

### 6.4 Mouse and drag

`MouseDown` sets the caret (Shift extends), `MouseMove` with a button extends the selection, and a timer autoscrolls while the pointer is above or below the view. Dragging a selection out of the view starts a drag with the same `Message` as the clipboard (Section 8). Dropping onto the view inserts it at the drop position, which moves the text if the drop came from the same view.

### 6.5 Notifications

Copy the `TextView` event mask so apps already know it: `EI_CONTENT_CHANGED`, `EI_CURSOR_MOVED`, `EI_SELECTION_CHANGED`, `EI_FOCUS_LOST`, `EI_WAS_EDITED`, plus one new `EI_STYLE_CHANGED`, sent when the style at the caret changes, so the toolbar can update its Bold button and font menu. Messages go through the `Control` invoker with an `"events"` field, as `TextEdit::CommitEvents()` does.

## 7. Public API

```cpp
namespace os {

class RichTextView : public Control
{
public:
    RichTextView( const Rect& cFrame, const String& cName, uint32 nResizeMask = CF_FOLLOW_LEFT | CF_FOLLOW_TOP,
                  uint32 nFlags = WID_WILL_DRAW );

    // Content
    void        SetPlainText( const String& cText );
    String      GetPlainText() const;
    status_t    SetHTML( const String& cHtml );        // through the html translator (Section 9)
    String      GetHTML() const;
    status_t    Import( const void* pData, size_t nLen, const String& cMimeType );   // any reader translator
    status_t    Export( const String& cMimeType, std::vector<uint8>& cOut,
                        const Message* pcConfig = NULL, Message* pcReport = NULL );  // "lost" in pcReport
    rt::Document&       GetDocument();
    const rt::Document& GetDocument() const;

    // Selection and caret, in rt::Position (paragraph, byte offset)
    void        Select( const rt::Range& cRange );
    rt::Range   GetSelection() const;
    void        SetCursor( const rt::Position& cPos, bool bExtendSelection = false );
    rt::Position GetCursor() const;
    void        MakeCursorVisible();

    // Formatting the selection (or the typing style if the selection is empty)
    void        SetBold( bool );          void ToggleBold();
    void        SetItalic( bool );        void ToggleItalic();
    void        SetUnderline( bool );     void ToggleUnderline();
    void        SetStrikethrough( bool );
    void        SetFontFamily( const String& cFamily );
    void        SetFontSize( double vPoints );
    void        SetTextColour( const Color32_s& );
    void        SetHighlight( const Color32_s& );
    void        SetBaseline( int nNormalSuperSub );
    void        SetLink( const String& cHref );       // empty removes it
    rt::CharStyle GetStyleAtCursor() const;           // "mixed" fields for a mixed selection

    // Paragraph formatting for every paragraph the selection touches
    void        SetParagraphStyle( const String& cNamed );   // "Heading 1" ...
    void        SetAlignment( int nAlign );
    void        SetIndent( double vLeft, double vRight, double vFirst );
    void        SetList( int nType );                         // none, bullet, number, checkbox
    void        SetCodeBlock( const String& cLanguage );      // empty turns code off
    void        SetInlineCode( bool );
    void        InsertHorizontalRule();
    void        SetMarkdownShortcuts( bool );
    void        SetSyntaxTheme( const String& cTheme );       // "light", "dark"
    void        IndentList( int nDelta );

    // Insertion
    void        InsertText( const String& cText );            // with the typing style
    void        InsertImage( Bitmap* pcBitmap, const String& cSource );

    // Editing state
    void        Cut(); void Copy(); void Paste();
    void        Undo(); void Redo();
    bool        CanUndo() const; bool CanRedo() const;
    void        SetReadOnly( bool ); bool GetReadOnly() const;
    void        SetMaxUndoSize( int ); int GetMaxUndoSize() const;
    bool        IsModified() const; void SetSavePoint();

    // Appearance
    void        SetPageMargins( double vLeft, double vTop, double vRight, double vBottom );
    void        SetWrapWidth( double vPoints );   // 0 = wrap to the view
    void        SetEventMask( uint32 ); uint32 GetEventMask() const;

    // Find
    bool        Find( const String& cText, bool bForward, bool bCaseSensitive, bool bWrap );
    int         ReplaceAll( const String& cFind, const String& cReplace, bool bCaseSensitive );

    virtual Point GetPreferredSize( bool bLargest ) const;
};

}
```

`rt::` types appear in the public header on purpose. A `(line, column)` `IPoint` like `TextView`'s doesn't mean anything once lines wrap and the column is a byte offset.

## 8. Clipboard and drag and drop

`os::Clipboard` holds an `os::Message`, so one copy can offer several formats, and the paste side picks the best it understands:

| Field | Content | Reader |
|---|---|---|
| `text/x-syllable-richtext` | the rich text stream from Section 9.2 (exact runs, paragraph styles, images) | other `RichTextView`s, and any app through the translators |
| `text/html` | the HTML subset from Section 9.7, with code colours kept | Webster and anything HTML-aware |
| `text/plain` | UTF-8 text, paragraphs joined with `\n` | `TextView`, Terminal, everything |

Paste prefers the first, then `text/html` (through the `html` translator, the same one used for files, so styles from other apps survive as far as the subset allows), then `text/plain` with the typing style. Paste-as-plain-text (Ctrl+Shift+V) always uses `text/plain`. Drag and drop uses the same `Message`.

## 9. File formats through the translator system

Every format except PDF goes through Syllable's translator framework, the one the image formats already use. Scribe never hard-codes a file format. It asks the `TranslatorFactory` for a translator from the file's MIME type to a rich text stream, or the other way, so adding a format later means dropping a `.so` into the translators directory.

### 9.1 How the framework works today

From `translation/translator.h` and `libsyllable/translation/translator.cpp`:

- A translator add-on exports `get_translator_count()` and `get_translator_node( int )` (see `translators/png/pngtrans.cpp:477-495`). `TranslatorFactory::LoadAll()` loads every library in `/system/extensions/translators`.
- A `TranslatorNode` says what it converts (`TranslatorInfo`: `source_type`, `dest_type`, `quality`), answers `Identify( src, dst, data, len )` by sniffing the first bytes, and creates `Translator`s.
- A `Translator` is fed with `AddData( data, len, final )` and read with `Read()`, with an optional `SetConfig( Message )` for options.
- Images all go through one intermediate type, `TRANSLATOR_TYPE_IMAGE` (`"image/x-atheos_trans"`): a `BitmapHeader` and frames. Readers convert PNG, JPEG and so on into it, and writers convert out of it. Every translator in `system/sys/translators/` is an image translator today.

Rich text uses the same pattern with its own intermediate type.

### 9.2 The rich text stream

Add `TRANSLATOR_TYPE_RICHTEXT = "text/x-syllable-richtext"` and a header, `translation/richtextstream.h` in `libos`, with two small helper classes, `RichTextStreamWriter` and `RichTextStreamReader`. They're the equivalent of `BitmapHeader`: translators use them and never link `librichtext`.

The stream is a sequence of length-prefixed records after a `"RTS1"` magic and a version number:

| Record | Content |
|---|---|
| `META` | key/value pairs: title, author, source language, generator |
| `STYLE` | a numbered character style as key/value pairs (`family`, `size`, `weight`, `italic`, `code`, `fg`, `link` ...) |
| `PARA` | paragraph style as key/value pairs (`named`, `align`, `indent_left`, `list_type`, `list_level`, `code_language` ...), then the paragraph's UTF-8 text |
| `RUN` | `start`, `end`, style number, for the current paragraph |
| `OBJECT` | byte offset of its U+FFFC, kind (`image`, `rule`), MIME type, source path, and the encoded bytes (PNG for images) |
| `END` | end of document |

Styles are key/value pairs, not fixed structs, so a newer Scribe can add a property and an older translator skips keys it doesn't know. The same stream is the `text/x-syllable-richtext` clipboard and drag field (Section 8), so copy and paste between two `RichTextView`s and file I/O share one serialiser.

`librichtext` has `rt::WriteStream( const Document&, ... )` and `rt::ReadStream( ..., Document& )` on top of the helpers. That's the only place the model meets the translators.

### 9.3 Open, save and export

**Open:** read the first 4 KB, get the MIME type from the registrar (extension, then sniffing), and call `FindTranslator( mime, TRANSLATOR_TYPE_RICHTEXT, data, len, &trans )`. Feed it the file with `AddData()`, read the stream with `Read()`, and build the document with `rt::ReadStream()`. Text translators simply buffer until `final` and convert in one go, which the API allows.

**Save and export:** `rt::WriteStream()`, then `FindTranslator( TRANSLATOR_TYPE_RICHTEXT, mime, ... )`, `AddData()` the stream, and write what comes out of `Read()`. Options go in through `SetConfig()`, for example `"images"` (`"embed"` or `"files"`), `"dest_path"` (so a writer can put images next to the document), `"keep_code_colours"` and `"html_fallback"`. A writer that drops things the format can't hold reports them in a `"lost"` string list in its reply, which Scribe shows before saving (Section 9.6).

**Menus build themselves.** At startup Scribe walks `GetTranslatorCount()`/`GetTranslatorInfo()`. Every node whose `dest_type` is the rich text type is something it can open, and every node whose `source_type` is the rich text type goes into File > Export. `TranslatorInfo` has no display name or extension, so the names and extensions come from the registrar's entry for the MIME type (`system/image/registrar-types.json`).

### 9.4 One problem to fix first: libos and libsyllable translators

Every translator links `libsyllable` (for example `translators/png/CMakeLists.txt`: `target_link_libraries(pngtrans PRIVATE libsyllable syllable::png)`), and `libos`'s copy of `TranslatorFactory` loads from the same `/system/extensions/translators` directory (`libos/translation/translator.cpp:69`). A `libos` app that loads one of them also loads `libsyllable`, so the process gets two copies of every `os::` class, including `Translator` itself. At best `dynamic_cast` and RTTI break. At worst it crashes on the first call across the two.

The fix is small: `libos`'s factory loads from `/system/extensions/translators/os/` instead, and a CMake helper builds each translator twice, once against each library, from the same source. The rich text translators are built only for `libos`. It should be checked whether Syllable's loader would even accept the duplicate symbols, but the separate directory is right either way.

### 9.5 Translators to ship

All in `system/sys/translators/text/`, each one a reader and a writer in one add-on:

| Add-on | Types | Notes |
|---|---|---|
| `plaintext` | `text/plain` | UTF-8, with a Latin-1 fallback when the input isn't valid UTF-8. One paragraph per line. Code blocks and lists are written as indented text. |
| `html` | `text/html` | The native format (Section 9.7). libxml2's `htmlReadMemory()` for reading. Links libxml2, so `librichtext` doesn't have to. |
| `markdown` | `text/markdown`, `text/x-markdown` | Section 9.6 |
| `rtf` | `text/rtf`, `application/rtf` | Reader for the common subset (`\b`, `\i`, `\ul`, `\strike`, `\fs`, `\f` with the font table, `\cf` with the colour table, `\par`, `\pard`, `\ql/qc/qr/qj`, `\li/ri/fi`, `\line`), about 600 lines. The writer is simpler. |

Later candidates: an ODT writer (zip of XML; zlib is already in the ports), a DOCX reader, and man pages. **PDF stays in the app** (Section 11), because it needs Pango layout and pagination from `librichtext`. It could later become a translator that links `librichtext`, which would give every app a "rich text to PDF" path.

### 9.6 Markdown

There are three parts.

**1. The translator.** Port **cmark-gfm**, GitHub's fork of the CommonMark reference parser. It's C99, BSD-licensed, builds with CMake and has no dependencies, and it adds the GitHub extensions people expect: strikethrough, task lists, tables and autolinks. Build it as a static `syllable_port()` (a new `cmake/ports/35-text.cmake`) and link it into the `markdown` translator. Reading walks cmark's AST and emits the stream. Writing is our own code, because the mapping back is specific to the model.

| Markdown | Document |
|---|---|
| `#` to `######`, Setext headings | Heading 1-6 (the stylesheet grows from 3 to 6 headings) |
| paragraph | Body |
| `*em*`, `**strong**`, `~~strike~~` | italic, bold, strikethrough |
| `` `code` `` | `code` character style (Section 3.2) |
| fenced or indented code block | Code paragraphs, one per line, with `code_language` from the info string (` ```cpp `) |
| `>` blockquote | Quote; nested quotes add indent levels |
| `-`, `*`, `1.` lists, start number | list type, level and start number |
| `- [ ]`, `- [x]` | checkbox list item, unchecked or checked |
| `[text](url)`, autolinks | link |
| `![alt](path)` | image object, loaded relative to the file through the image translators |
| hard line break | U+2028 inside the paragraph (Pango breaks the line there) |
| `---` thematic break | rule object |
| tables | v1: one paragraph per row, cells separated by tabs, and a warning. Proper tables come with table support. |
| raw HTML | v1: kept as literal text |

Writing reverses the table and backslash-escapes Markdown's special characters (backslash, `*`, `_`, `[`, `]`, `#`, backtick, `+`, `-`, `!`) wherever they'd otherwise mean something. Markdown can't hold underline, colours, fonts, sizes, alignment or highlight, so the writer drops them and lists them in `"lost"`. Scribe then asks "Saving as Markdown will remove: underline, text colour. Save anyway?" With `"html_fallback"` on, underline and colour are written as inline `<u>` and `<span style>` instead, which most Markdown renderers accept. The round trip that has to be exact is Markdown to document to Markdown: the output should be the input, normalised. The CommonMark spec examples make a ready test suite.

**2. Markdown typing shortcuts in the editor**, behind `SetMarkdownShortcuts( bool )`, on by default in Scribe:
- At the start of a paragraph: `#` to `######` plus space make a heading; `-`, `*` or `+` plus space a bullet list; `1.` plus space a numbered list; `[ ]` plus space a checkbox; `>` plus space a quote; ` ```lang ` plus Enter a code block in that language; `---` plus Enter a rule.
- Inline: `**text**` becomes bold, `*text*` or `_text_` italic, `` `text` `` code and `~~text~~` strikethrough, when the closing marker is typed.
- Each conversion is its own undo step, so Ctrl+Z straight after one brings back the characters you typed. This is the usual behaviour in editors that do this, and it's what makes the feature bearable when it fires by mistake.

These are input rules that run after inserting a trigger character (space, Enter, `*`, `_`, `` ` ``, `~`) and look at the text just before the caret. They don't involve the translator.

**3. Markdown source view.** View > Markdown Source shows the document as Markdown text, highlighted with a Markdown syntax format (Section 10). Switching to it writes the document through the Markdown translator into a single code block in language `markdown`. Switching back reads it in again. Anything Markdown can't hold gets the same warning first. This is the mode for people who'd rather type the Markdown, and it comes nearly free once Sections 9.6 and 10 exist.

### 9.7 Native format

The native format stays **HTML** (the `html` translator above): Webster can open the files, and it holds everything in the model, including colours, fonts and alignment, which Markdown can't. One element per paragraph (`<p>`, `<h1>`-`<h6>`, `<blockquote>`, `<pre><code class="language-cpp">`, `<li>` inside `<ul>`/`<ol>`), runs as `<b>`, `<i>`, `<u>`, `<s>`, `<sup>`, `<sub>`, `<code>`, `<a href>`, and `<span style>` for family, size and colour. A `<meta name="generator" content="Scribe 1">` lets the reader know it's reading its own output. Pango markup was considered and rejected: no paragraph attributes, lists or images.

A document opened from Markdown saves back to Markdown by default, so editing a README doesn't turn it into HTML.

The registrar should list Scribe as an "open with" handler for `text/html`, `text/markdown` and `text/rtf`, and as the default for `text/markdown` and `text/rtf`. Webster stays the default for `text/html`.

## 10. Source code highlighting

### 10.1 What gets highlighted

Code blocks: consecutive paragraphs with the Code style and the same `code_language`. A code block gets a monospace font, a tinted background band across the full width, no first-line indent, tab stops every 4 characters (`pango_layout_set_tabs()`, measured from the width of a space), and syntax colouring. Inline `code` spans get the monospace font and a background but no colouring, because a few words don't say what language they are.

The language is set by a Markdown fence info string, by `class="language-x"` in HTML, from the file extension when a source file is pasted or dropped in, or by the user from a language dropdown that appears on the toolbar while the caret is in a code block.

### 10.2 Reuse CodeView's formats

`libcodeview` (`system/libs/codeview`) already has highlighters for C/C++, Java, Perl, Ruby and HTML (`format_c.cpp`, `format_java.cpp`, `format_perl.cpp`, `format_ruby.cpp`, `format_html.cpp`), and CodeEdit uses them. The interface, `cv::Format` in `format.h`, fits a paragraph model exactly:

```cpp
virtual CodeViewContext Parse( const os::String& cText, os::String& cStyle, CodeViewContext nContext ) = 0;
```

It takes one line and the context left by the line before (inside a comment, inside a string, and so on), fills `cStyle` with one style number per byte, and returns the context for the next line. One code line is one paragraph here, so each Code paragraph is parsed with the context from the paragraph above it. It's byte-based, which matches the model's byte offsets. `GetIndentString()` gives auto-indent on Enter for free.

`libcodeview.so` links `libsyllable`, so a `libos` app can't load it (the same problem as Section 9.4). Move the `format_*` files and `format.h` into a small new library, **`libsyntax`** (`system/libs/syntax`), which only needs `os::String` and `os::Color32_s`, and build it for both `libos` and `libsyllable`. Then `libcodeview` links the `libsyllable` build and nothing changes for CodeEdit. `libsyntax` also gets:

- a registry from language names, aliases and file extensions to format factories: `cpp`, `c++`, `c`, `h`, `hpp`, `java`, `perl`, `pl`, `ruby`, `rb`, `html`, and so on;
- new formats over time, written to the same interface: Markdown first (for the source view in Section 9.6), then Python, shell, JSON, CMake and diff. Each is a few hundred lines in the style of `format_c.cpp`.

### 10.3 Highlighting is a layer, not formatting

Syntax colours aren't stored in the document as runs. If they were, every keystroke in a code block would create undo entries and file formats would fill up with colour spans. `LayoutCache` keeps them per paragraph instead:

```cpp
struct SyntaxState {
    std::string styles;          // one style number per byte, from Format::Parse()
    CodeViewContext contextIn;   // context the paragraph was parsed with
    CodeViewContext contextOut;  // context it handed to the next paragraph
    uint32_t revision;           // paragraph revision it was computed for
};
```

They're never saved and never undone. When building a code paragraph's attribute list (Section 4.1), the cache turns runs of equal style numbers into foreground (and optionally bold or italic) attributes, added after the paragraph's own runs, so the syntax colour wins over text colour. Bold or italic the user applied still shows.

**Incremental reparse.** After an edit in code paragraph *k*, reparse *k*. If its `contextOut` is unchanged, stop. If it changed (the user just typed `/*`), carry on to *k+1*, and so on until a paragraph's new `contextOut` matches what it was before or the block ends. That's the same thing CodeView does, and it means typing an opening `/*` recolours the rest of the block while normal typing reparses one line. For very long blocks, do the follow-on paragraphs in batches from the same idle timer as background layout (Section 4.2).

**Colour themes.** Each format numbers its own styles (`GetStyleCount()`, `GetStyleName()`). A theme maps those per-format style names (comment, keyword, string, number, preprocessor ...) to colours, with defaults for a light page and a dark one. Scribe can read CodeEdit's format colour settings so the two apps match, if CodeEdit keeps them somewhere that's simple to read.

### 10.4 Editing inside a code block

- Tab inserts a tab (or spaces, by setting) instead of changing the list level, and Shift+Tab removes one level of indent from the selected lines.
- Enter starts a new Code paragraph with `GetIndentString()` from the line above. Enter on an empty last line of the block leaves the block, the usual way out.
- Paste into a code block always pastes plain text, one paragraph per line.
- Markdown typing shortcuts and smart quotes are off.
- Long lines wrap with `PANGO_WRAP_WORD_CHAR`, with a small marker at the right edge. The other choice is no wrapping and a horizontal scrollbar per block, which is better for code but much more work in a document view, so leave it for later.

### 10.5 Export

- **HTML:** `<pre><code class="language-cpp">`. With `"keep_code_colours"` on, the writer also wraps tokens in `<span style="color:...">`, so the colours survive pasting into mail or a web form. The `text/html` clipboard field always has this option on.
- **Markdown:** a fenced block with the language as its info string.
- **RTF:** the colours as `\cf` runs.
- **PDF:** nothing extra to do, because PDF is drawn from the same layouts, colours included.

## 11. Printing and PDF

Make a cairo PDF surface (`cairo_pdf_surface_create()`, the port is built with it) at the page size in points. Make a separate `LayoutCache` at 72 DPI with the page's text width. Then paginate: walk paragraphs and lines with `PangoLayoutIter`, start a new page when the next line's logical rectangle doesn't fit, and draw lines with `pango_cairo_show_layout_line()` at their y on the page. Widow and orphan control and "keep with next" for headings come later.

Printing is the same PDF passed to CUPS once the `40-cups.cmake` port is in use. Until then, Export as PDF is the print story.

## 12. Scribe, the app

Use `aedit` (`system/apps/utils/aedit`) as the template for the app skeleton (`appwindow`, `buffer`, find and replace dialogs, `messages.h`, catalogs), with a `RichTextView` where it has its `EditView`.

- **Window**: menu bar, toolbar, the view, a status bar (word count, page count from the PDF paginator, current style name).
- **Toolbar**: paragraph style dropdown, a language dropdown shown while the caret is in a code block, font family dropdown (from `pango_font_map_list_families()` under `TextLock`), size spinner, Bold/Italic/Underline toggles, text colour, alignment, bullet and numbered list. They update from `EI_STYLE_CHANGED`.
- **Menus**: File (New, Open, Open Recent, Save, Save As, Export with one entry per writer translator plus PDF, Close), Edit (Undo, Redo, Cut, Copy, Paste, Paste as Plain Text, Select All, Find, Find Next, Replace), Format (Font..., Bold, Italic, Underline, Strikethrough, Superscript, Subscript, Text Colour, Highlight, Link, Clear Formatting, Paragraph Style, Alignment, Indent, Lists), Insert (Image, Code Block, Horizontal Rule, Page Break later), View (Markdown Source, Zoom later, Show Page Width).
- **Page width**: by default wrap at a fixed text width (6.5 inches at 96 DPI) centred in the window, like a page, with the window background around it. A setting switches to wrapping at the window width.
- **Open from the desktop**: open the files named on the command line, the way `AEditApp` does with `argv` (`aedit/main.cpp:41-82`).

## 13. Fixes needed on the `cairo` branch first

From reading the code. None are large.

1. **`libos` builds**, with a CMake target that links the ports from `30-cairo.cmake`. This is the main blocker, and it's shared with every other use of the branch.
2. **`TextLock()`**: add the process-wide recursive lock to `libos/graphics/` and take it in `TextLayout`, `GFont::GetPangoFont()` and `GraphicsBackend::DrawTextLayout()` (Section 4.4).
3. **`TextLayout::SetFont()` leaks** the `PangoFontDescription` from `pango_font_description_from_string()` (`textlayout.cpp:140`), because `pango_layout_set_font_description()` copies it. Free it after the call.
4. **`TextLayout` contexts**: each `TextLayout` creates its own `PangoContext` with no resolution or font options (`:45-53`). Give `libos` one shared context with the resolution and font options from Section 4.3, and have `TextLayout` use it, so all `libos` text measures the same.
5. **`GraphicsImage( Bitmap* )`** passes `LockRaster()` to cairo and then calls `UnlockRaster()` straight away (`graphics_image.cpp:97-103`, and the same in the other two constructors). Harmless today, since the raster is shared memory that doesn't move, but it should hold the lock for the surface's life, or document that it doesn't need to. It also hard-codes `CAIRO_FORMAT_ARGB32`. Use `CAIRO_FORMAT_RGB24` for `CS_RGB32` bitmaps so cairo doesn't treat the unused byte as alpha.
6. **Server colour swap** (`graphics_cairo.cpp:229`, `:433`): pass `nRed, nGreen, nBlue`. The editor doesn't use the server path, but every other widget's text and strokes are wrong until this is fixed.
7. **Translator directory for `libos`** (Section 9.4): load from `/system/extensions/translators/os/` and build translators for both libraries.
8. **`GraphicsImage::CreateFromFile()`** ignores its argument and opens `"picture.png"` (`graphics_image.cpp:39`). Not needed by the editor (it loads images through the translators), but worth fixing while in the file.

## 14. Milestones

Each milestone ends with something that runs.

| # | Milestone | Done when | Size |
|---|---|---|---|
| M0 | Prerequisites | `libos` builds under CMake, Section 13 items 2-5 are in, and a test app draws a `PangoLayout` with two attributes into a `GraphicsImage` and blits it | M, mostly the branch build |
| M1 | Model on the host | `rt::Document`, runs, styles, the four primitives, undo, plain text I/O, with unit tests running on the Mac | M |
| M2 | Read-only view | `RichTextView` shows a hard-coded styled document, wraps on resize, scrolls, and stays correct with two windows open (threading) | M |
| M3 | Plain editing | caret, all of Section 6.1, typing, Backspace/Delete, mouse selection, autoscroll, undo/redo, `text/plain` clipboard | L |
| M4 | Character formatting | typing style, B/I/U/S, family, size, colours, super/subscript, links; `EI_STYLE_CHANGED`; richtext clipboard format | M |
| M5 | Paragraph formatting | alignment, indents, spacing, named styles, bullet and numbered lists | M |
| M6 | Translators and Scribe | rich text stream, the `libos` translator directory, the `plaintext` and `html` translators with round-trip tests, and the app with menus, toolbar, open/save through translators, find/replace and modified state | L |
| M7 | Code blocks | `libsyntax` split out of `libcodeview` (CodeEdit still builds), Code paragraphs, language dropdown, incremental highlighting, themes, code export in HTML | M |
| M8 | Markdown | cmark-gfm port, `markdown` translator with CommonMark spec round trips, loss warning, typing shortcuts, Markdown format in `libsyntax` and the source view | L |
| M9 | Images | inline images through the shape renderer, insert from file, `<img>` and `![]()` | S |
| M10 | Output | PDF export with pagination; `rtf` translator | M |

Out of scope for v1: tables (Markdown tables import as tab-separated rows), columns, footnotes, comments and track changes, spell checking (no spell library is ported yet), OpenType features, and vertical text.

## 15. Testing

- **Host unit tests for the model** (`tests/`, built with the host compiler from the Mac or Linux, no Syllable): inserting and deleting across paragraph boundaries; run splitting and merging; applying a style over a range that partly covers runs; undo and redo of each primitive restoring the document byte for byte; typing coalescing; `Fragment` copy and paste round trips; HTML read → write → read giving the same document.
- **Host translator tests**: each translator's convert functions are plain C++ taking bytes and producing the stream, so they build on the host too. Run the CommonMark spec examples through Markdown → document → Markdown and check the output is the normalised input; same for HTML and RTF fixtures.
- **Host highlighting tests**: typing `/*` in the first line of a 200-line C block recolours every line below; typing `*/` restores them; a normal keystroke reparses exactly one paragraph.
- **Host layout tests**: the `layout/` code compiled against the host's Pango. Check hit-test round trips (`index → x,y → index`) over sample paragraphs in English, Arabic and Hindi, and caret movement over combining marks. Modern Pango gives slightly different metrics, so test relationships, not pixel positions.
- **On Syllable**: a scripted test app that loads a fixture document, sends key events, and dumps the document and selection after each step, compared against expected files. Then a manual pass: two windows typing at once (the lock), resize while a 5,000-paragraph document lays out, paste from Webster, open the saved file in Webster.
- **Performance budget** in the 1000 MHz QEMU guest: a keystroke in a 500-word paragraph should relayout and repaint in under 16 ms. Measure M3 against that before going on, because if one paragraph's relayout is too slow, it changes the design (split long paragraphs into layout chunks).

## 16. Open questions

1. **Name.** "Scribe" is a placeholder.
2. **Where the widget lives.** This plan puts it in `system/libs/richtext`. If other `libos` apps will want it (mail, notes), it could go straight into `libos/gui/` instead, which is now easier, because libxml2 moved into the `html` translator.
3. **Native format.** HTML is recommended above, with Markdown files saving back as Markdown. If you'd rather have a format that can hold things HTML can't (track changes, comments), that should be decided before M6.
4. **Markdown flavour.** cmark-gfm (GitHub's extensions) is recommended. Plain cmark is smaller but has no strikethrough, task lists or tables.
5. **Highlighter source.** Splitting CodeView's formats into `libsyntax` is recommended. The other option is porting a larger existing highlighter (GNU source-highlight, or tree-sitter for incremental parsing), which gives many more languages but brings Boost or a C runtime per grammar.
6. **Starting before the branch builds.** Path 2 needs only cairo, Pango and a bitmap, and `main` already builds those libraries. If the `libos` build drags on, M1 and most of M2-M3 could be developed on `main` against libsyllable, with small local copies of `TextLayout` and `GraphicsImage`, and moved to `libos` later. The model is unaffected either way.
7. **Screen DPI.** Nothing in the appserver reports a DPI today. 96 is assumed above; a setting in `AppserverConfig` would make point sizes match paper.
