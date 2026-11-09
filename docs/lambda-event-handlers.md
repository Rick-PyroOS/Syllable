# Lambda event handlers for views (libos, `cairo` branch)

Plan only. No code has been changed. Everything below was read from the `cairo` branch (`83e0a251`) and `feat/input-server` (`7e122756`) in `~/Code/Rick-PyroOS/Syllable`, plus `inputserver-gap-audit-20261102T031604-6c5a.md`.

## 1. Goal

Let app code attach behaviour to a view without subclassing it:

```cpp
auto* pcIcon = new os::ImageView( cFrame, "icon", pcImage );

pcIcon->on_mouse_enter( [=]( const os::MouseEvent& ) { pcIcon->SetStyleState( os::HOVERED ); } );
pcIcon->on_mouse_leave( [=]( const os::MouseEvent& ) { pcIcon->SetStyleState( os::NORMAL ); } );
pcIcon->on_single_click( [=]( const os::MouseEvent& e ) { Select( pcIcon ); } );
pcIcon->on_double_click( [=]( const os::MouseEvent& e ) { Open( pcIcon ); } );

os::TimerId nBlink = pcCaret->on_timer_tick( 530000, [=]() { pcCaret->Toggle(); } );
...
pcCaret->remove_timer( nBlink );
```

The existing virtual model (`MouseDown`, `MouseMove`, `MouseUp`, `WheelMoved`, `KeyDown`, `Handler::TimerTick`) stays as it is and keeps working. Lambdas are an extra layer that runs next to it.

Names: Rick's message used `on_mouseenter`, `on_mouseclick`, `on_single/double_click` and `on_timer_tick`. This plan normalises them to one word per underscore (`on_mouse_enter`, `on_click`, `on_single_click`, `on_double_click`, `on_timer_tick`). The rest of libos is CamelCase (`MouseDown`). If you'd rather match that (`OnMouseEnter`), it is a rename and changes nothing else here.

## 2. What the code does today

These are the facts the design depends on.

**Target library.** On `cairo` the new GUI library is `system/sys/appserver/libos` (`libos.so.1`, headers under `system/sys/include/pyro/pyro/`), linked against cairo and pango. `libsyllable` sits next to it unchanged apart from the `ToolTip` edits in `gui/view.cpp`. This plan targets libos. Section 9.4 covers backporting to libsyllable.

**Mouse dispatch** (`libos/gui/window.cpp`):

- `Window::DispatchMessage()` handles `M_MOUSE_DOWN` / `M_MOUSE_UP` (`:1343`, `:1380`). It hit-tests with `FindView()` and calls the virtual `MouseDown()`/`MouseUp()` on the deepest view under the pointer. If nothing was hit, it calls the focus child. `MouseUp` also goes to the focus child when that is a different view.
- The base `View::MouseDown/MouseUp/WheelMoved` (`view.cpp:1290`, `:1332`, `:1358`) forward to the parent's virtual. Bubbling depends on overrides calling the base class, and many don't.
- `M_MOUSE_MOVED` goes through `_MouseEvent()` → `_CallMouseMoved()` (`:1160-1230`). Each view stores a `MOUSE_INSIDE`/`MOUSE_OUTSIDE` mode, and gets `MOUSE_ENTERED`/`MOUSE_EXITED` on a change. This is DOM `mouseover`/`mouseout` behaviour: a parent gets `MOUSE_EXITED` when the pointer moves onto one of its children, because `m_pcLastMouseView` is always the deepest view.
- Moves are coalesced. If another `M_MOUSE_MOVED` is queued, the current one is skipped. There is a small bug here: line `:1436` reads `_transit` from `pcMsg` instead of `pcNextMsg`, so the "same transition" test always passes and every queued move is dropped. That's harmless for positions, but a view that the pointer crosses quickly can miss its enter/exit pair completely.

**Double click today.** Nothing in the client library detects it centrally. `ListView::MouseDown` (`listview.cpp:860`) and `IconView` (`iconview.cpp:1175`) each compare `get_system_time()` against a hard-coded `500000`. The appserver has a user setting (`AppserverConfig::m_nDoubleClickDelay`, `config.cpp:43`, served by `DR_GET_MOUSE_CFG` in `server.cpp:756`), but libos never asks for it. `os::Application` has `GetKeyboardConfig()` but no mouse equivalent.

**Event messages.** `InputNode::EnqueueEvent()` (`appserver/server/input.cpp:130`) stamps every input event with `_qualifiers` and `_timestamp` before it is routed. `SrvWindow::MouseDown()` (`swindow.cpp:1149`) copies the whole event into the message it sends to the app window. So the client already receives the hardware-ish press time in `M_MOUSE_DOWN`/`M_MOUSE_UP`. Today it ignores it.

**Timers** (`libos/util/looper.cpp`):

- `Looper::AddTimer(Handler*, int nID, bigtime_t, bool bOneShot)` and `RemoveTimer()` keep a sorted `TimerNode` list. The looper thread calls the virtual `Handler::TimerTick(nID)` with the looper locked (`:1741`).
- `RemoveTimer()` correctly checks both the pending list and the "restart" list, so a repeating timer can cancel itself from its own tick.
- `Looper::RemoveHandler()` (`:890`) **does not remove that handler's timers**. A view with a running timer that is detached and deleted leaves a `TimerNode` pointing at freed memory. The next tick is a use-after-free. This is a bug today, and lambda timers would make it far more common, so the fix is step 1 of the plan.

**Pimpl and reserved slots.** `View`, `Window`, `Looper` and `Application` all keep their state behind `class Private; Private* m;`. `View` has `__VW_reserved2__` to `__VW_reserved20__`, `Handler` has five reserved virtuals plus `uint32 __res[6]`, `Control` has five. `libos.so.1` already differs from libsyllable's vtable (it inserts `Paint(const Rect&, Graphics&)` and `SetStyle()`), and it hasn't shipped yet.

**Input server.** On `feat/input-server` the input server sources are present but don't build yet, and nothing routes their events into the appserver (audit section 3). The audit's recommended path is to forward them into `InputNode::EnqueueEvent()`. That makes `EnqueueEvent()` the one place that every pointer event passes through, from both the old in-appserver drivers and the new input server. Section 5.4 uses that.

**Compiler.** Per Rick (2026-11-08): libos and the appserver move to GCC 9.5.0 before this work starts, so the plan assumes C++14 (`-std=gnu++14`, GCC 9.5's default) everywhere libos is built or used. GCC 4.1.2 compatibility isn't a goal, and `pyro/` headers may use C++11/14 freely.

## 3. Design summary

1. **Handlers are plain `std::function`s.** `on_click( std::function<void( const MouseEvent& )> )` and the other `on_*` functions are ordinary exported members, so any lambda, functor or `std::bind` result works. All of libos and its apps are built with GCC 9.5, so passing `std::function` across the library boundary is safe (section 9.2).
2. **Dispatch moves into one non-virtual path per event.** `Window` calls `View::_DeliverMouseDown()` and the other `_Deliver*` functions instead of the virtuals directly. Each one runs the view's lambdas first, then the virtual, unless a lambda accepted the event. The base `View::MouseDown()` bubbles by calling `parent->_DeliverMouseDown()`, so parent lambdas fire on bubble too.
3. **Clicks are synthesised in `Window`,** from press/release pairs on the same view, with a click count taken from `_timestamp`, a position slop and the user's double-click delay. Later the click count is computed in `InputNode::EnqueueEvent()` and sent as `_clicks`.
4. **Enter/leave uses hierarchy semantics** (DOM `mouseenter`/`mouseleave`). `Window` keeps the chain of views under the pointer and diffs it on each move. The existing `MOUSE_ENTERED`/`MOUSE_EXITED` codes passed to the `MouseMove` virtual don't change.
5. **Lambda timers live in the `Looper`.** A `TimerNode` can carry a callback instead of an ID. An owner handler is recorded so that `RemoveHandler()` can purge timers. Views that aren't attached yet queue their timers and arm them in `_Attached()`.
6. **Threading follows the existing rule.** Callbacks run on the window's looper thread with the window locked, the same as the virtuals. There is one new helper, `Looper::Post(fn)`, for getting back onto that thread from elsewhere.
7. **ABI:** no vtable or class layout changes. Only new non-virtual exported functions, and new state in the `Private` classes.

## 4. Public API

### 4.1 Event structs (`pyro/gui/events.h`, new)

```cpp
namespace os {

enum view_event_kind
{
    VE_MOUSE_DOWN, VE_MOUSE_UP, VE_MOUSE_MOVE,
    VE_MOUSE_ENTER, VE_MOUSE_LEAVE,
    VE_CLICK, VE_SINGLE_CLICK, VE_DOUBLE_CLICK,
    VE_WHEEL,
    VE_KEY_DOWN, VE_KEY_UP,           // phase 5
    VE_COUNT
};

struct ViewEvent
{
    uint32      nKind;          // view_event_kind
    View*       pcTarget;       // view the event was first delivered to
    View*       pcCurrent;      // view whose handlers are running (changes while bubbling)
    bigtime_t   nTimestamp;     // from "_timestamp"; dispatch time if missing
    uint32      nQualifiers;    // from "_qualifiers"
    mutable bool bAccepted;     // set by Accept(); stops the virtual and bubbling

    void Accept() const { bAccepted = true; }
    uint32 __res[4];
};

struct MouseEvent : public ViewEvent
{
    Point       cPosition;      // in pcCurrent's coordinates
    Point       cScreenPosition;
    uint32      nButton;        // 1 = left, 2 = right, 3 = middle (Syllable numbering)
    uint32      nButtons;       // mask of buttons held
    int         nClickCount;    // 1, 2, 3... (clicks and downs); 0 otherwise
    int         nTransit;       // MOUSE_ENTERED / INSIDE / EXITED / OUTSIDE for moves
    Message*    pcDragData;     // "_drag_message", or NULL
    Point       cWheelDelta;    // VE_WHEEL only
    uint32 __res[4];
};

}
```

The library builds these and passes them by `const&`. Apps don't allocate them, so fields can be added at the end, or put into `__res`, without breaking old binaries. Copying one by value in app code is fine: an old app copies only the fields it knows about.

The type isn't called `Event` because `os::Event` already exists (`util/event.h`, the registrar's remote event API).

### 4.2 Handler types (`pyro/gui/events.h`)

```cpp
#include <functional>

namespace os {

typedef std::function<void( const MouseEvent& )> MouseHandler;
typedef std::function<void()>                    TimerHandler;

typedef int32 HandlerId;          // > 0; 0 means "failed"
typedef int32 TimerId;

}
```

There's no custom callback class. `std::function` already gives type erasure, copy/move, and small-buffer storage for captures that fit, so a capture of a pointer or two doesn't allocate. Key events (phase 5) get their own `KeyHandler` typedef with a `KeyEvent`.

### 4.3 View methods

```cpp
class View : public Handler
{
public:
    HandlerId on_mouse_down  ( MouseHandler fn );
    HandlerId on_mouse_up    ( MouseHandler fn );
    HandlerId on_mouse_move  ( MouseHandler fn );
    HandlerId on_mouse_enter ( MouseHandler fn );
    HandlerId on_mouse_leave ( MouseHandler fn );
    HandlerId on_click       ( MouseHandler fn );
    HandlerId on_single_click( MouseHandler fn );
    HandlerId on_double_click( MouseHandler fn );
    HandlerId on_wheel       ( MouseHandler fn );

    bool      remove_handler( HandlerId nId );
    void      remove_all_handlers();

    // timers: declared on Handler (section 7), shown here for completeness
    TimerId   on_timer_tick( bigtime_t nPeriod, TimerHandler fn );   // repeating
    TimerId   after        ( bigtime_t nDelay,  TimerHandler fn );   // one-shot
    bool      remove_timer ( TimerId nId );
};
```

Each `on_*` is a one-line exported function that calls a private `_AddHandler( view_event_kind, MouseHandler&& )`. Named functions read better in code and in docs than one generic entry point, and the cost of a few extra exported symbols doesn't matter while `libos.so.1` is unshipped.

The functions return an id, not `*this`. Removal is the common need: toggling hover feedback, cancelling a blink. A chaining builder can be added later as a thin wrapper.

A lambda that doesn't need the event still has to declare it (`[]( const os::MouseEvent& ) { ... }`). Section 10 asks whether to add template overloads that also accept `[] { ... }`.

`on_timer_tick` lives on `Handler`, not only `View` (section 7), so `Window` and `Application` subclasses get it as well.

## 5. Clicks, single click and double click

### 5.1 Definitions

- **Press target:** the view that received `VE_MOUSE_DOWN` for button *b*.
- **Click:** button *b* is released while the pointer is over the press target or one of its descendants, and the pointer moved less than the *drag slop* (4 px, Chebyshev distance in screen coordinates) since the press. A larger move is a drag, so no click fires.
- **Click count:** a press continues the previous sequence if it uses the same button, hits the same view, comes within *double-click delay* of the previous **press**, and lands within the slop of the first press. Otherwise the count resets to 1. The count is set on `VE_MOUSE_DOWN`, so the rich-text editor can select a word on the second press (`rich-text-editor-design.md`, double/triple click). It is repeated on the matching `VE_CLICK`.
- **Delay:** the user's setting from the appserver (`DR_GET_MOUSE_CFG` → `"doubleclick"`). It is fetched once and cached in `Application::Private`. Add `Application::GetMouseConfig( float*, float*, bigtime_t*, bool* )` as a non-virtual member, mirroring `GetKeyboardConfig()`. There is no change notification for this setting (keyboard config has none either), so re-fetch it lazily, for example at most once per second when a press arrives, so a change in Preferences takes effect without restarting apps. Default to 500 ms if the request fails.
- **Time source:** `_timestamp` from the message, not `get_system_time()` at dispatch. On a busy window, or on the 1 GHz single-core QEMU setup in the README, dispatch can lag the press by enough to turn a double click into two single clicks.

### 5.2 Which callbacks fire

For a double click on a view that has all three kinds of callback:

| Input | Fires |
|---|---|
| press 1 | `on_mouse_down` (count 1) |
| release 1 | `on_mouse_up`, `on_click` (count 1). `on_single_click` is **armed**, not fired |
| press 2 (within delay) | `on_mouse_down` (count 2). The armed single click is **cancelled** |
| release 2 | `on_mouse_up`, `on_click` (count 2), `on_double_click` |

With no second press, `on_single_click` fires once the delay has passed since press 1, as a one-shot looper timer.

These rules decide how it behaves in practice:

- **`on_single_click` waits only if the view also has `on_double_click` callbacks.** Otherwise it fires on release, like `on_click`, so a view that only wants single clicks doesn't pay a 500 ms delay. The cost is that `on_single_click` timing depends on what else is registered. That's the right trade-off, but it needs to be documented.
- `on_double_click` fires on the second **release**, matching DOM `dblclick` and the definition of a click. Apps that want to act on the second press (as Finder-style icon views do) use `on_mouse_down` and check `nClickCount == 2`. Section 10 lists this as an open question.
- Triple clicks and beyond are available through `nClickCount` on `on_click`/`on_mouse_down`. There is no dedicated `on_triple_click`.
- The pending single-click timer is owned by the view (section 7), so deleting the view cancels it.

### 5.3 Where it lives

Add to `Window::Private`:

```cpp
struct PressState { ViewRef cView; uint32 nButton; Point cScreenPos; bigtime_t nTime; int nCount; };
PressState  m_asPress[3];          // one per button
ViewRef     m_cLastClickView;      // for continuing a sequence
bigtime_t   m_nLastPressTime;  Point m_cLastPressPos;  uint32 m_nLastButton;  int m_nLastCount;
```

`ViewRef` is the liveness handle from section 8.3, so a view deleted between press and release just drops the click.

`M_MOUSE_DOWN` → compute the count → `_DeliverMouseDown()`. `M_MOUSE_UP` → `_DeliverMouseUp()`, then, if it's a click, `_DeliverClick()`. That function fires `VE_CLICK` and then `VE_DOUBLE_CLICK` or `VE_SINGLE_CLICK`, or arms the latter. Clicks bubble up through ancestors' click callbacks until a callback calls `Accept()`. There is no virtual for clicks today. Section 9.3 suggests using one reserved slot to add one.

### 5.4 Input server and appserver: `_clicks`

Phase 1 counts clicks in the client from `_timestamp`, so it works on `cairo` alone. Phase 3 moves the count to the server:

- `InputNode::EnqueueEvent()` (`appserver/server/input.cpp:130`) already stamps `_timestamp`/`_qualifiers`, and is the planned entry point for input server events. Add `_clicks` to `M_MOUSE_DOWN`/`M_MOUSE_UP` there, using `AppserverConfig::GetDoubleClickTime()` and the screen position. The count then matches across the whole desktop: window-border double click to raise (`swindow.cpp:1279`), the dock and apps.
- The server can't know which **view** was hit, so the client still resets the count if the view changed. It takes `min(server _clicks, same-view count)`.
- The client uses `_clicks` when it is present and falls back to its own count when it isn't. That way, a libos built for the input server still runs against an older appserver.
- When the input server owns devices, it should set `_timestamp` at **device read time**, in each device add-on such as `ps2mouse.cpp:187`. `EnqueueEvent()` then keeps an existing `_timestamp` instead of overwriting it. Today it would add a second one, because `AddInt64` appends. So the change is "only add if missing", and the client should read index 0.
- Also fix while there: the old drivers send both `_button` and `_buttons` on press/release, marked "To be removed". Leave both in place until libos reads only `_button` for press/release and `_buttons` for moves, which is what `window.cpp` already does.

## 6. Enter and leave

Hierarchy semantics: a view is "hovered" while the pointer is over it **or any descendant**. `on_mouse_enter` fires once when the pointer comes into that set, and `on_mouse_leave` fires once when it goes out. Moving from a parent onto its child fires nothing on the parent. That's what hover styling needs (the gallery's `os::HOVERED` state, which no header defines yet).

Algorithm, in `Window::_MouseEvent()` after the existing `_CallMouseMoved()` calls:

1. `cNew` = path from the top view down to `FindView( pos )`, or empty if the window transit is `MOUSE_EXITED` or the pointer is outside the top view.
2. Find the longest common prefix with `m->m_cHoverChain`.
3. Fire `VE_MOUSE_LEAVE` on old views below the prefix, **deepest first**.
4. Fire `VE_MOUSE_ENTER` on new views below the prefix, **outermost first**.
5. Store `cNew` (as `ViewRef`s).

Enter/leave don't bubble (each view in the chain gets its own), and `Accept()` has no effect on them.

Edge cases:

- **View detached or hidden while hovered:** `_ViewDeleted()` / hide removes it and its descendants from the chain **without** firing leave, the same as DOM. Call it out in the docs, because hover styling on a reused view has to reset in `DetachedFromWindow()`.
- **Window deactivates or pointer leaves the window:** the transit becomes `MOUSE_EXITED`, the chain empties, and leave fires.
- **Button held (drag):** the chain still updates, so `on_mouse_leave` fires while dragging out of a button. `Button` uses this to un-highlight.
- **Coalesced moves:** enter/leave reflect state at the moves that are processed. A view crossed between two processed moves gets no pair. Fix the `pcNextMsg` typo in section 2, so moves with a different transit aren't dropped.
- The `MouseMove` virtual keeps its current `MOUSE_ENTERED`/`MOUSE_EXITED` codes (mouseover/mouseout style), so existing widgets don't change.

`on_mouse_move` fires for every processed move on the hovered view and on the focus child, the same calls as the virtual, with `nTransit` set.

## 7. Timers

### 7.1 Looper changes

```cpp
struct TimerNode                     // private nested type; only libos allocates it
{
    Handler*        m_pcHandler;
    int             m_nID;
    TimerHandler    m_cCallback;     // NEW: if set, called instead of TimerTick()
    bool            m_bRunning;      // NEW: set while its callback runs
    bool            m_bDead;         // NEW: removed while running; delete after return
    ...
};

TimerId Looper::AddTimer( Handler* pcOwner, bigtime_t nPeriod, bool bOneShot, TimerHandler fn );   // overload
bool    Looper::RemoveTimer( TimerId nId );                                                       // overload
void    Looper::_RemoveTimersFor( Handler* pcOwner );     // called from RemoveHandler()
```

- **Ids** come from a per-looper counter, starting at `0x40000000` and stored in `m_nID`, so they can never collide with the small integers apps pass to `AddTimer()`. `Handler::TimerTick()` is never called for them, which means a subclass that overrides `TimerTick()` without calling the base can't swallow lambda ticks. That's why the callback lives in the node, rather than going through `TimerTick` with a reserved id range.
- **`RemoveHandler()` calls `_RemoveTimersFor()`.** This fixes the existing use-after-free for plain `AddTimer()` timers as well. Land it on its own first.
- **Running node:** the loop at `:1720` has already unlinked the node (one-shot) or moved it to the restart list (repeating) before the call. If the callback removes its own timer, or deletes its owner, the node is marked `m_bDead` and freed after the call returns, instead of being freed under the running callback. That matters more with `std::function`, because destroying it destroys the lambda's captures while the lambda is still running. For one-shots, move the `std::function` out of the node into a local before calling it.
- Exceptions: wrap the call in `try { } catch( ... ) { dbprintf(...); }`. libos builds with `-fexceptions`, and an exception unwinding into the looper loop would leave it unlocked.

### 7.2 View and Handler wrappers

`Handler::on_timer_tick()`, `after()` and `remove_timer()` are exported, non-virtual members that forward to `GetLooper()->AddTimer( this, ... )`. They need nothing new in `Handler`'s layout. `View` changes the behaviour only for the not-yet-attached case:

- If `GetLooper()` is NULL, the callback is queued in `View::Private::m_cPendingTimers` with a provisional id from a global atomic counter. `_Attached()` arms the queued timers, mapping provisional ids to real ones in a small `std::map`.
- On `_Detached()`, `RemoveHandler()` purges them. **Repeating timers don't survive a re-attach.** That's simpler and avoids ticks on a hidden view, but it's a behaviour choice; section 10 lists it.
- `~View` drops any pending timers and frees their callbacks.

## 8. Threading with Loopers

### 8.1 Rule

Every view callback, mouse or timer, runs on the **owning window's looper thread with the window locked**. That's the same contract as `MouseDown()` and `TimerTick()` today, so a callback can touch its own window's views freely.

### 8.2 Registering

- **Unattached view** (being built before `AddChild`): no lock needed.
- **Attached view:** the caller must hold the window lock, or be on its thread. `_AddHandler()` checks `GetLooper()->GetLockingThread() == get_thread_id( NULL )` and prints a `dbprintf` warning in debug builds. This is the same requirement as any other `View` call, so it adds no new rule, but registering from a worker thread is the first thing people will get wrong.
- The callback table is `std::vector< std::pair<HandlerId, MouseHandler> >` per kind, in `View::Private`. It is only touched under the window lock.

### 8.3 Re-entrancy and lifetime

Callbacks will remove handlers, add handlers, close windows and delete views. The dispatcher has to survive that:

- **Iterating:** iterate by index over the kind's vector and re-check its size after each call. Removal clears the slot's id, marking it dead, and defers erasing it (and so destroying the `std::function`) until the outermost dispatch on that view finishes, tracked by a dispatch-depth counter in `View::Private`. Callbacks added during a dispatch don't run in that dispatch.
- **View deleted from its own callback** (`on_click` → `delete pcDialogView`): `View::Private` holds a ref-counted `ViewLife { int nRefs; bool bAlive; }`. `ViewRef` is a small handle to it. The dispatcher takes a ref, calls each callback, and stops as soon as `bAlive` is false. `~View` sets `bAlive = false` and releases its own ref. The same handle protects `Window`'s press state and hover chain. The rule for app code: deleting the **sender** inside its callback is safe; deleting the **window** should go through `PostMessage( M_QUIT )` as today.
- **Captures:** a lambda that captures its own view pointer is safe, because the callback is owned by the view and dies with it. A lambda that captures **another** view can dangle. Recommend storing a `ViewRef` (exposed publicly as `os::ViewRef`, with `Get()` returning NULL once the view is gone) when the other view can go away first.

### 8.4 Crossing windows and threads

Calling into another window from a callback means locking it while holding your own. Two windows doing that to each other deadlock. Add:

```cpp
status_t Looper::post( TimerHandler fn );          // exported, callable from any thread
```

`post` moves `fn` into a heap `TimerHandler` and sends an internal `M_POST_CALLBACK` message carrying the pointer. `Looper::DispatchMessage()` calls and deletes it, under that looper's lock. If the looper quits first, `~Looper` drains the queue and deletes pending callbacks without running them. This is also the safe way for worker threads (a file loader, a network thread) to update the UI:

```cpp
pcWindow->post( [=] { pcStatus->SetText( "Loaded" ); } );
```

## 9. ABI

### 9.1 libos.so.1 (new, unshipped)

It's tempting to break things freely while libos is unshipped, but the plan doesn't need to:

- **No new virtuals and no vtable reordering.** Everything is non-virtual exported functions plus state in `View::Private`, `Window::Private`, `Looper::Private` and `Application::Private`.
- **`Handler` stays the same size.** It has no pimpl, and this design adds only non-virtual member functions to it.
- **`TimerNode`** is declared in `looper.h` but only constructed by libos, so changing it is safe.
- **The `Window`/`View` dispatch change is internal.** Apps built before the change keep working, because their overrides are still called the same way.

### 9.2 Compiler and `std::function` across the boundary

With libos, the appserver and every libos app built by GCC 9.5.0 against the same libstdc++ (`libstdc++.so.6` from the 9.5 port), it's safe to pass `std::function` and `std::vector` through exported functions. These are the rules that keep it safe:

- **One libstdc++ ABI setting for the whole image.** `_GLIBCXX_USE_CXX11_ABI` changes `std::string` and `std::list` layouts. `std::function` itself isn't affected, but other `std::` types in libos signatures are, so the image has to be consistent. Use whatever the GCC 9.5 port was configured with, and don't let individual Makefiles override it.
- **Pin `-std=gnu++14`** in the libos Makefile/CMake and in the app template. It's the 9.5 default, but pinning stops a later GCC from silently changing the language mode under the headers.
- **No GCC 4.1.2-built code links against libos.** The new headers include `<functional>` and won't parse there. That's acceptable per the upgrade plan, but say it in the libos README.
- `std::function`'s layout has been stable in libstdc++ since GCC 4.x, so a later move from 9.5 to a newer GCC with the same `libstdc++.so.6` major doesn't break it.

### 9.3 Reserved slots, if you want parity for subclasses

Subclasses could also get a click hook without lambdas, by using two of `View`'s 19 spare slots:

- `__VW_reserved2__` → `virtual void MouseClicked( const MouseEvent& cEvent );` (default: nothing)
- `__VW_reserved3__` → `virtual void HoverChanged( bool bHovered );` (hierarchy enter/leave)

Replacing a reserved virtual in place keeps the vtable layout. Old binaries never call the slot, so they're unaffected. Optional, phase 4.

### 9.4 Backport to libsyllable.so.7

The layout side is additive in the same way: new exported symbols, `Private` state, the internal dispatch change and the `TimerNode` fields. So it can be backported without changing the soname, and old apps are unaffected. The catch is `std::function` in the signatures: libsyllable and the apps that use the new calls have to be built with GCC 9.5 too. Unchanged GCC 4.1.2-built apps keep working, because they never reference the new symbols or headers. Do it after libos has settled, as a straight port of the same files.

## 10. Open questions

1. **Naming:** `on_mouse_enter` (this plan) or libos's CamelCase `OnMouseEnter`?
2. **Double click on second release or second press?** The plan picks release, and press is available through `nClickCount` on `on_mouse_down`.
3. **Do repeating timers survive detach/re-attach?** The plan says no.
4. **Should `on_click` also fire for keyboard activation** (Space/Enter on a focused `Button`)? It probably belongs to a separate `Control::on_invoked( fn )` that wraps `Control::Invoked()`. That's useful, but out of scope here.
5. **Lambdas without a parameter** (`[]{ ... }` for `on_click`): add a small template overload that wraps it into a `MouseHandler`?

## 11. Phases

Each phase builds and runs on its own.

1. **Timer safety fix:** `RemoveHandler()` purges timers, and fix the `pcNextMsg` `_transit` typo. Small, separate commits on `cairo`.
2. **Core:** pin `-std=gnu++14` in the libos build, `events.h`, the `View::on_*` registration functions, the `_Deliver*` dispatch path, `ViewLife`/`ViewRef`, mouse down/up/move/wheel callbacks, timer handlers in `Looper`, and `Looper::post`.
3. **Clicks and hover:** press state, client-side click count with `_timestamp`, `Application::GetMouseConfig()`, single/double click, hover chain enter/leave. Replace the hard-coded `500000` in `ListView`/`IconView` with the shared delay.
4. **Input server integration** (after `feat/input-server` is routed into `InputNode::EnqueueEvent()`): `_clicks` in `EnqueueEvent()`, device-time `_timestamp` kept when present.
5. **Optional:** reserved-slot virtuals, `Control::on_invoked`, `on_key_down`/`on_key_up`, the libsyllable backport.

**Testing.** Add a gallery page ("Events") with a view per feature that logs callbacks to a `ListView`: hover chain on nested views, click vs drag slop, single vs double vs triple, a blinking timer that removes itself, a button that deletes itself in `on_click`, and a worker thread that posts. Run it on QEMU with the delay set to 200 ms and 800 ms in Preferences. For the threading rules, also run a stress case with 50 views each holding a 10 ms timer, detached and deleted in a loop, under the debug allocator. That is the case that crashes today.

## 12. Animations (`on_animation_frame` and `animate()` with Tweeny)

Added 2026-11-09, after Rick said Tweeny is the likely animation library. [Tweeny](https://github.com/mobius3/tweeny) is header-only, MIT-licensed C++11. A tween is a value range with a duration and an easing, and `tween.step( dt )` advances it and returns the current value. It has no clock of its own, so libos supplies the clock and Tweeny stays a header-only dependency of apps.

### 12.1 API

Two layers. The low-level one is exported from libos and knows nothing about Tweeny:

```cpp
struct AnimationFrame
{
    bigtime_t nNow;        // get_system_time() at this frame
    bigtime_t nDelta;      // µs since this callback's previous frame (0 on the first)
    bigtime_t nElapsed;    // µs since this callback started
    int       nFrame;      // 0, 1, 2...
};
typedef std::function<bool( const AnimationFrame& )> FrameHandler;   // return false to stop
typedef int32 AnimationId;

class View
{
public:
    AnimationId on_animation_frame( FrameHandler fn );
    AnimationId on_animation_frame( const String& cKey, FrameHandler fn );   // replaces any running one with the same key
    bool        cancel_animation( AnimationId nId );
    bool        cancel_animation( const String& cKey );
};
```

The Tweeny layer is an inline template in a separate header, `pyro/gui/animation.h`, which includes `<tweeny.h>`. Apps that don't animate never see Tweeny, and libos's ABI doesn't depend on a Tweeny version:

```cpp
template<class... T, class Apply>
AnimationId animate( View* pcView, const String& cKey, tweeny::tween<T...> cTween,
                     Apply fnApply, std::function<void()> fnDone = {} );
```

It wraps the tween in an `on_animation_frame` callback. Each frame steps the tween by the elapsed milliseconds, calls `fnApply( values... )`, then calls `Invalidate()` on the view. When the tween reaches the end it calls `fnDone` and returns `false`.

Hover fade with a key, so leaving halfway through an enter animation picks up from the current value instead of jumping:

```cpp
float vGlow = 0.0f;
pcButton->on_mouse_enter( [=, &vGlow]( const os::MouseEvent& ) {
    os::animate( pcButton, "glow",
        tweeny::from( vGlow ).to( 1.0f ).during( 150 ).via( tweeny::easing::quadraticOut ),
        [&]( float v ) { vGlow = v; } );
} );
pcButton->on_mouse_leave( [=, &vGlow]( const os::MouseEvent& ) {
    os::animate( pcButton, "glow",
        tweeny::from( vGlow ).to( 0.0f ).during( 250 ),
        [&]( float v ) { vGlow = v; } );
} );
```

(Real code would keep `vGlow` in the view subclass or in a `std::shared_ptr`, not on the stack. The example only shows the call shape.)

### 12.2 Frame clock

- **One clock per window.** `Window::Private` holds the active frame callbacks for all its views. A single repeating looper timer runs while that set is non-empty and stops when it empties, so an idle window costs nothing. It uses the `std::function` timer path from section 7.
- **One flush per frame.** Each tick runs every view's frame callbacks first, then calls `Flush()` once, so ten animating views cost one round trip to the appserver, not ten.
- **Driven by real time.** `nDelta` is measured with `get_system_time()`, not assumed. On the 1 GHz QEMU setup a slow frame makes the animation skip ahead instead of running slowly, and every animation still ends on time.
- **Rate:** 60 Hz by default (16.67 ms period), set per app with `Application::SetAnimationRate()`. The appserver has no vsync or frame signal today. If it gets one later (for example a `M_FRAME` sent after the cairo backend presents), the clock switches to it without any API change.
- **Reduced motion:** a desktop setting that makes `animate()` jump straight to the end value and call `fnDone`. That's cheap to add now, and it makes animation-heavy UI testable and fast on slow machines.

### 12.3 Tweeny gotchas the wrapper hides

- **`step()` is overloaded on the argument type.** `step( int32_t )` advances by duration units, but `step( float )` advances by a **fraction of the whole tween**. Passing `nDelta / 1000.0f` would make every animation finish on the first frame. The wrapper casts to `int32_t` milliseconds and carries the sub-millisecond remainder forward, so the steps don't drift.
- Durations are unitless in Tweeny. The wrapper's convention is **milliseconds**, documented in `animation.h`.
- `tween.step()` on a finished tween keeps returning the end value. The wrapper checks `progress() >= 1.0f` to stop.

### 12.4 Lifetime and threading

Frame callbacks follow the same rules as everything else in this plan. They run on the window thread with the window locked, are owned by the view, are cancelled on detach and deletion, and are safe against a callback that cancels itself or deletes its view (section 8.3). A cancelled animation doesn't call `fnDone`. Starting one from a worker thread goes through `pcWindow->post( ... )`.

## 13. Gestures (`on_pinch`, `on_swipe_up`, and so on)

Added 2026-11-09, after Rick mentioned libinput and gesture support.

### 13.1 Follows the init server design, section 2.19

The input side is already decided in `init-server-design-2026-11-08.md` section 2.19, and this section only adds the app-facing layer on top. In short, from there:

- **The real freedesktop libinput runs inside inputserver.** It uses the path backend (no udev, with a libudev stub as on FreeBSD), libevdev and mtdev, a patch so its event loop uses `poll()` instead of epoll/timerfd, and a GCC 9.5 build through `cmake/ports/`.
- **Drivers expose an evdev-compatible node.** This applies to `usb_hid` with multitouch usages, a Synaptics/Elantech PS/2 touchpad driver, later I²C-HID, and a Bluetooth HID add-on.
- **libinput recognises the gestures:** 3+ finger swipes, pinches and holds. Two-finger scrolling stays scrolling and becomes wheel messages.
- **The appserver handles system gestures first,** from a binding table, the way it handles hotkeys. By default three-finger left/right switches virtual desktop and three-finger up opens an overview. Everything else goes to a window.
- **Testing** uses your Magic Trackpad through a Mac-side `MultitouchSupport` forwarder and a `virtual_touchpad` driver (recommended), or USB passthrough.

What I checked on `cairo` agrees with it. `usb_mouse` is a boot-protocol relative mouse, `ps2aux` handles relative IntelliMouse packets only, and nothing reports more than one contact, so the kernel work in 2.19 comes before any of this can run on hardware.

### 13.2 Two changes to the init design's gesture plan

1. **Message field names.** 2.19 lists `phase`, `fingers`, `dx`, `dy`, `scale`, `angle`, `time`. Every other input message uses underscore names (`_scr_pos`, `_button`, `_timestamp`, `_qualifiers`), and `InputNode::EnqueueEvent()` already adds `_timestamp`. I'd use `_phase`, `_fingers`, `_delta` (Point), `_delta_unaccel` (Point), `_scale`, `_angle`, `_scr_pos`, and drop `time` in favour of `_timestamp`. That's a small edit to the init doc's table if you agree.
2. **`View::GestureEvent()` virtual.** 2.19 proposes `virtual void GestureEvent( const os::GestureEvent& )`. A method with the same name as the struct type works in C++, but only if every use spells the type as `os::GestureEvent`, which is confusing. I'd name the virtual `GestureReceived( const GestureEvent& )` and put it in a reserved slot (`__VW_reserved4__`), so the vtable layout doesn't change. Lambdas run first, then the virtual, unless a lambda calls `Accept()`, the same as the mouse handlers. The default implementation keeps the init design's Ctrl+pinch → zoom shortcut fallback for old apps.

### 13.3 Pipeline and messages

libinput (in inputserver) → `M_GESTURE_SWIPE` / `M_GESTURE_PINCH` / `M_GESTURE_HOLD` → `InputNode::EnqueueEvent()` (stamps `_timestamp`, `_qualifiers`) → appserver binding table (system gestures stop here) → the focused window, as 2.19 says → `Window` picks the view (section 13.5) → lambdas, then `GestureReceived()`.

| Code | Fields |
|---|---|
| `M_GESTURE_SWIPE` | `_phase`, `_fingers`, `_delta`, `_delta_unaccel`, `_scr_pos` |
| `M_GESTURE_PINCH` | `_phase`, `_fingers`, `_delta`, `_delta_unaccel`, `_scale` (1.0 at begin), `_angle` (degrees since last update), `_scr_pos` |
| `M_GESTURE_HOLD` | `_phase`, `_fingers`, `_scr_pos` |

`_phase` is `GESTURE_BEGIN`, `GESTURE_UPDATE`, `GESTURE_END` or `GESTURE_CANCEL`, matching libinput's begin/update/end events and its `cancelled` flag on end.

### 13.4 API

```cpp
struct GestureEvent : public ViewEvent
{
    int    nPhase;         // GESTURE_BEGIN / UPDATE / END / CANCEL
    int    nFingers;
    Point  cPosition;      // pointer, in pcCurrent's coordinates
    Point  cDelta;         // since the previous update
    Point  cTotal;         // since BEGIN
    float  vScale;         // pinch: cumulative, 1.0 at BEGIN
    float  vAngleDelta;    // pinch: degrees since the previous update
    Point  cVelocity;      // px/s, valid at END
    uint32 __res[4];
};
typedef std::function<void( const GestureEvent& )> GestureHandler;

class View
{
public:
    // continuous: every phase
    HandlerId on_swipe( GestureHandler fn );
    HandlerId on_pinch( GestureHandler fn );
    HandlerId on_hold ( GestureHandler fn );

    // discrete: once, at END, if the gesture passes the thresholds below
    HandlerId on_swipe_up   ( GestureHandler fn );
    HandlerId on_swipe_down ( GestureHandler fn );
    HandlerId on_swipe_left ( GestureHandler fn );
    HandlerId on_swipe_right( GestureHandler fn );
    HandlerId on_pinch_in   ( GestureHandler fn );   // fingers together, vScale < 1
    HandlerId on_pinch_out  ( GestureHandler fn );   // fingers apart, vScale > 1 (Rick's "on_expand")
};
```

They're registered and removed with the same `HandlerId` machinery as the mouse handlers (`remove_handler()`), with new `VE_GESTURE_*` kinds.

Naming: "pinch" alone is ambiguous between the two directions, so the discrete pair is `on_pinch_in` / `on_pinch_out`. `on_expand` could be added as an alias for `on_pinch_out` if you prefer that word (open question below).

**Discrete thresholds** (defaults, all in one `GestureConfig` struct the app can change):

- **Swipe direction:** the dominant axis of `cTotal` must be at least 2× the other axis, and the swipe must cover at least 50 px or end faster than 300 px/s. A swipe that fails both is continuous-only.
- **Pinch:** `vScale` at END below 0.8 for `on_pinch_in`, above 1.25 for `on_pinch_out`.
- **Finger count:** the discrete swipe callbacks fire for any count of 3 or more by default. `GestureEvent::nFingers` lets a handler react only to 3 or only to 4 fingers.
- A `GESTURE_CANCEL` (for example, a finger added mid-gesture) never fires a discrete callback.

### 13.5 Delivery

- **Which view:** the appserver picks the window (the focused one, per 2.19). Inside it, the target is the view under the pointer if the pointer is over that window, and otherwise the focus child.
- **Target latching:** the target is chosen at `GESTURE_BEGIN` and held (as a `ViewRef`) until END/CANCEL. That's the same idea as the press target for clicks. If it's deleted mid-gesture, later phases are dropped.
- **Claiming:** BEGIN bubbles from the target up through ancestors until a handler calls `Accept()`. That view becomes the gesture's owner, and the remaining phases go straight to it. If no view accepts BEGIN, the rest of the phases still bubble per event, so a plain `on_swipe_left` on a parent works without anyone calling `Accept()`.
- **Discrete callbacks** are evaluated at END on the owner, or on the bubble chain if there is no owner. They run after the continuous `on_swipe` / `on_pinch` END callback.
- **System gestures** never reach apps. The appserver's binding table from 2.19 takes them first (three-finger left/right and up by default). So the discrete `on_swipe_left/right/up` callbacks mainly see four-finger swipes, or three-finger swipes when the user has unbound the system defaults. That's worth saying in the API docs.

### 13.6 Animations and gestures together

The two features are meant to be used together. Continuous updates drive the UI directly, and the END phase hands off to a tween:

```cpp
pcImage->on_pinch( [=]( const os::GestureEvent& e ) {
    if( e.nPhase == os::GESTURE_UPDATE ) {
        pcImage->SetZoom( m_vZoomAtBegin * e.vScale );       // follows the fingers
    } else if( e.nPhase == os::GESTURE_END ) {
        float vTarget = SnapZoom( pcImage->GetZoom() );       // 100%, 200%, fit...
        os::animate( pcImage, "zoom",
            tweeny::from( pcImage->GetZoom() ).to( vTarget ).during( 200 )
                .via( tweeny::easing::cubicOut ),
            [=]( float v ) { pcImage->SetZoom( v ); } );
    }
} );
```

Swipe-to-dismiss and kinetic scrolling follow the same pattern, using `cVelocity` at END to choose the tween's duration.

### 13.7 Open questions for sections 12 and 13

1. `on_pinch_out` or `on_expand` (or both)?
2. Should two-finger swipes also trigger `on_swipe_*`? libinput treats them as scrolling, and this plan follows that. A two-finger "flick" could be derived from fast scroll ends instead.
3. Do you agree with the two changes to the init design in 13.2 (underscore field names, and `GestureReceived` in a reserved slot)? If so, the init doc's table should be updated to match.
4. Default animation rate on QEMU: 60 Hz, or 30 Hz until the emulated CPU is faster?

### 13.8 Phases (continuing section 11)

6. **Animation:** frame clock in `Window`, `on_animation_frame`, `animation.h` with Tweeny as a port (header-only, so a `cmake/ports` entry that only installs headers), and the reduced-motion setting. This only depends on phase 2.
7. **Gesture API:** message codes, `GestureEvent`, the `on_*` gesture handlers, `GestureReceived()`, and the window-side routing. This can be built and tested before libinput exists by having a test app post synthetic `M_GESTURE_*` messages to its own window.
8. **End to end:** once the init design's M11 work exists (evdev node, libinput in inputserver, the appserver binding table) along with the Magic Trackpad forwarder, connect it to this. No libos change should be needed at that point.
