# Mesa 3D for Syllable (Pyro)

OpenGL for Syllable: Mesa 6.5.2's software renderer, a GLUT, and Mesa's demos.

| What | Where on the boot volume | From |
|---|---|---|
| `libGL.so.6`: OpenGL 1.5 + extensions, software rendering, with the OSMesa API | `/system/libraries` | MesaLib 6.5.2, built with Mesa's own make and `configs-syllable` |
| `libGLU.so.1`: the SGI GLU | `/system/libraries` | the same tree |
| `libGLView.so.1`: `os::GLView`, an `os::View` you render into with GL | `/system/libraries` | `glview/` |
| `libglut.so.3`: GLUT 3.7 | `/system/libraries` | `glut/glut_syllable.cpp`, plus GLUT's fonts, shapes and teapot from MesaGLUT 6.5.2 |
| Headers (`GL/*.h`, `gui/GLView.h`) | `/system/development/headers` | |
| The Mesa demos (gears, morph3d, reflect, isosurf, ... 50 of them) | `/applications/MesaDemos/bin`, textures in `/applications/MesaDemos/images` | MesaDemos 6.5.2 `progs/demos`, unchanged |
| GLTest | `/applications/MesaDemos/GLTest` | the Mesa 6.0.1 port's example (Builder's `mesa-6.0.1`), unchanged |
| GLTeapot | `/applications/MesaDemos/GLTeapot` | Haiku's GLTeapot on `os::GLView` (`glteapot/`, see its README) |

The three Mesa archives are downloaded during the build (SourceForge, SHA-256
checked), the way the ports are.

## Build and try it

In the tree it is part of `sb build`. On its own, against a built tree:

```sh
cd ~/Code/Rick-PyroOS/Syllable-Build-System
./sb compile ../Syllable/system/libs/mesa      # -> out/compiled/mesa
./sb execute --put mesa                        # copy it into the running Syllable (a debug image)
./sb execute gears
```

On Syllable, from the Dock's menu: Other > Mesa, with GLTeapot, GLTest and the demos in
folders (Showcase, Shaders, Textures, Tests and Benchmarks; `os::Category`
attributes like `Other/Mesa/Showcase`, which need the Dock's nested
categories).  From a terminal: `/applications/MesaDemos/bin/gears`. Each demo
changes to its own folder first, because the demos open their data files
(`isosurf.dat`, `../images/*.rgb`) relative to the working directory.

`-DMESA_DEMOS="gears;morph3d"` builds only some of the demos.

## How it fits together

The Mesa 6.0.1 port (`Builder/packages/mesa-6.0.1`) had a Syllable device
driver in Mesa itself (`drivers/syllable/GLView.cpp`), written against Mesa
6.0's internal span-function interface. Mesa 6.5 replaced that interface with
renderbuffers, so the driver would need rewriting for every Mesa version. The
later Builder recipes (`mesa-6.5.2`, `Mesa3D-7.10.3`) only built OSMesa, the
off-screen interface, with nothing to put the result on screen, and GLUT was
never ported (its `src/glut/syllable` was BeOS's GLUT, renamed).

This port uses OSMesa, Mesa's public and stable API for rendering into memory
the caller owns, and does the Syllable side outside Mesa:

- **`os::GLView`** (`glview/`) keeps the 6.0.1 port's API (`GLParams`,
  `LockGL()`, `UnlockGL()`, `SwapBuffers()`, `CopySubBuffer()`), so code
  written for it, like GLTest, builds unchanged. Its back buffer is an
  `os::Bitmap` (`CS_RGB32`, shared with the appserver): OSMesa renders into
  its raster as BGRA, top row first (`OSMESA_Y_UP` 0, `OSMESA_ROW_LENGTH` the
  bitmap's row), and `SwapBuffers()` draws the bitmap into the view and
  `Sync()`s. The buffer follows the view's size.
- **GLUT** (`glut/glut_syllable.cpp`) runs the program's callbacks on the
  thread that calls `glutMainLoop()`, as GLUT programs expect. `glutInit()`
  starts the `os::Application` in a thread of its own; each GLUT window is an
  `os::Window` with a GLView, whose thread queues input, resizes and menu
  choices for the main loop. Supported: windows and sub-windows, display,
  reshape, keyboard (and key-up), special keys (arrows, F1-F12, Home/End/Page
  keys), mouse, motion, passive motion, entry, visibility, idle, timers, popup
  menus with sub-menus, `glutGet()`, `glutExtensionSupported()`,
  `glutGetProcAddress()`, fonts and shapes. Not supported: colour-index mode,
  overlays, cursors, pointer warping, game mode, joysticks and other devices.

- A window goes where `glutInitWindowPosition()` asks if it fits on the
  screen there with its title bar, below the Dock; otherwise (and with no
  position) it is centred, later windows cascading from there.  Programs that
  ask for (0, 0), as many demos do, get a centred window, not one with its
  title bar above the screen.

Mesa is built without thread-local contexts: one GL context is current per
process. GLUT renders from one thread; other programs hold `LockGL()` while
they render.

## Versions

Mesa 6.5.2 is what the Builder recipes had. Its source needs no changes for
today's compilers (it builds with GCC 13 as well as the toolchain's), needs only make, and its `osmesa` driver is
the same API in every later Mesa, so moving to Mesa 7.x (classic make) means
a new `configs-syllable` and nothing else here. Mesa 8 and later build with
SCons/Meson and need LLVM for a fast software rasterizer (llvmpipe); their
plain `softpipe` and OSMesa still work without it.
