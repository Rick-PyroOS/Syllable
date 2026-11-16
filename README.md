# Syllable Main Repo

---

This is the main repo that I will be putting changes in.

At some point this will be  merged into Pyro, but the structure
has changed so much that I want to be able to just focus on changes for now.


Building and running
---

Syllable is built in Docker by `sb` from
[Syllable-Build-System](https://github.com/Rick-PyroOS/Syllable-Build-System),
on macOS, Linux, or Windows through WSL 2. What to install, which
repositories to check out (this one, Syllable-Build-System, Syllable-Ports and
linux-afs) and the steps: [docs/build-and-run.md](docs/build-and-run.md).


Version
---

The version of Syllable (Pyro) is `"version"` in [package.json](package.json)
(`MAJOR.MINOR.PATCH`, optionally `-SUFFIX`; it starts at 0.0.1). CMake reads
it into `SYLLABLE_VERSION` (and `PROJECT_VERSION`), and `sb` names its disk
images and releases after it: `out/syllable-VERSION.qcow2`,
`syllable-VERSION.iso.gz`. Bump it there, and nowhere else.




Building appserver/libos
---
You will need: 
* gdk-pixelbuf-2.24.1 -> run `build gdk-pixelbuf-2.24.1 && build install gdk-pixelbuf-2.24.1`
* cairo-1.4.12 -> run `build cairo-1.4.12 && build install cairo-1.4.12`
* pango-1.20.2  -> run `build pango-1.20.2 && build install pango-1.20.2`
* librsvg - run `build librsvg-2.22.3 && build install librsvg-2.22.3`
* glib-2.14.0 - run `build glib-2.14.0 && build install glib-2.14.0`
* fontconfig-2.5.0 - run `build fontconfig-2.5.0 && build install fontconfig-2.5.0`
* expat-2.0.0 - run `build expat-2.0.0 && build install expat-2.0.0`



