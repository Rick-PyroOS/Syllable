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



