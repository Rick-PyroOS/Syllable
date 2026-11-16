# GLTeapot

Haiku's GLTeapot (`src/apps/glteapot` in the Haiku tree), ported to
Syllable (Pyro) on `os::GLView`.  Be Inc.'s 1999 sample, which Haiku changed
to turn its objects with quaternions.

- Left button: turn a teapot (let go while moving and it keeps spinning).
- Right button: move it (with Shift: nearer or further, in perspective).
- Middle button: its colour and solidity.
- The menus: add teapots; perspective, the FPS counter, filled polygons,
  lighting, backface culling, the Z-buffer, Gouraud shading, fog and the
  frame limit; three lights in six colours.

What changed for Syllable:

- `ObjectView` is an `os::GLView`.  Haiku's renders in a thread of its own;
  here the window's thread renders from a timer, so its GL calls and
  `GLView::SwapBuffers()` (which locks the window) are never in two threads.
- `TeapotWindow` is an `os::Window` (Haiku's is a `BDirectWindow`) with the
  same menus; the settings are `os::CheckMenu`s.
- The teapot, two raw resources in Haiku's `GLTeapot.rdef`, is
  `teapot_data.h`.
- "Limit FPS to refresh rate" is "Limit to 60 FPS": there is no wait for
  the screen's retrace.
- `util.h`: `this->` in `DoubleLinkedList` for GCC 4.1; `Vector3.h` and
  `Quaternion.h` include `<math.h>` and `<float.h>` themselves.

Licences: the Be sample code under the Be Sample Code License (`LICENSE`);
Haiku's changes under the MIT License (Copyright 2008 Haiku Inc.);
`Vector3.h` and `Quaternion.h`, from the Bullet physics library, under the
zlib licence in their headers.
