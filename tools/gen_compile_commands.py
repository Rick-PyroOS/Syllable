#!/usr/bin/env python3
"""
Generate compile_commands.json for the Syllable tree so clangd knows each
file's real include paths and defines.

Syllable is built on the Syllable box itself with plain Makefiles, so we can't
record a real build (no `bear`). Instead this reads every hand-written Makefile,
pulls the -I / -D / -U flags out of CFLAGS, CXXFLAGS, COPTS and friends, and
gives each source file the flags of the nearest Makefile above it, resolved
from that Makefile's directory, just as `make` would.

Base flags (target, macros, header tiers) come from tools/base_flags.txt
(userland) and tools/base_flags_kernel.txt (kernel and drivers). There are no
compile_flags.txt files: clangd reads only compile_commands.json, and gives a
file that isn't in it (a new file) the flags of its nearest neighbour.

Header lookup order: system/sys/include (Syllable's headers), then system/libs
and other repo library folders, then .sysroot/ (the box's glibc, libstdc++ and
third-party headers, copied by tools/sync_sysroot.py).

Usage (from anywhere):
    python3 tools/gen_compile_commands.py           # pulls .sysroot/ first if it's missing
    python3 tools/gen_compile_commands.py --sync    # refresh .sysroot/ from the box first
Re-run it after adding directories or changing Makefile include paths.
"""

import json
import os
import re
import shlex
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # .../Syllable
SYSROOT = os.path.join(ROOT, ".sysroot")                             # box headers (sync_sysroot.py)
KERNEL_DIR = os.path.join(ROOT, "system", "sys", "kernel")
# Kernel-side code: built with the kernel's flags, and its <inc/...>, <acpi/...>
# includes never leak onto userland's include path.
KERNEL_DIRS = [
    KERNEL_DIR,
    os.path.join(ROOT, "misc", "drivers"),
    os.path.join(ROOT, "system", "sys", "include", "syllable", "kernel"),
]


def is_kernel(dirpath):
    return any(dirpath == k or dirpath.startswith(k + os.sep) for k in KERNEL_DIRS)

# Directories that aren't built, or are source fragments (patches), or bring
# their own autotools build. clangd still opens their files with the fallback flags.
SKIP_DIRS = [
    ".git", ".idea", ".vscode", ".cache", ".sysroot", "objs",
    "system/libs/glibc", "system/libs/glibc-2.3.2",
    "system/sys/appserver/libatheos",        # legacy, not in appserver SUBDIRS
    "system/apps/utils/Builder/packages",    # patches for third-party packages
]

# Makefile variables we know the value of.
VARS = {
    "ATHEOS_SRC": os.path.join(ROOT, "system"),
    "ATHEOS_INCLUDE_PATH": os.path.join(ROOT, "system", "sys", "include", "syllable"),
    "ATHEOS_LIB_PATH": os.path.join(ROOT, "system", "libs"),
    "BUILD_TOOLS": os.path.join(ROOT, "system", "build-tools"),
}

# `$(shell ...)` commands and what they print on the Syllable box. Box-absolute
# paths are mapped into the local sysroot copy automatically.
SHELL_OUTPUT = {
    "freetype-config --cflags": "-I/usr/freetype/include/freetype2 -I/usr/freetype/include",
}

BOX_REFS = set()   # box-absolute -I folders the Makefiles use (filled by makefile_flags)

FLAG_VARS = re.compile(
    r"^\s*(?:export\s+)?(C|CXX|CPP|C_|CXX_|AM_C|AM_CPP|AM_CXX)?(FLAGS|OPTS|INCLUDES?|DEFINES|DEFS)\s*(\+|:|\?)?=\s*(.*)$"
)
SRC_EXT = {".c": "c", ".cc": "c++", ".cpp": "c++", ".cxx": "c++", ".C": "c++"}


def rel(path):
    return os.path.relpath(path, ROOT)


def skipped(dirpath):
    r = rel(dirpath)
    return any(r == s or r.startswith(s + os.sep) for s in SKIP_DIRS)


def read_base_flags(path):
    """base_flags*.txt -> list; '#' lines are comments, relative paths are from ROOT."""
    base_dir = ROOT
    out = []
    for line in open(path):
        f = line.strip()
        if not f or f.startswith("#"):
            continue
        for opt in ("-isystem", "-I"):
            if f.startswith(opt) and len(f) > len(opt):
                p = f[len(opt):]
                if not os.path.isabs(p):
                    f = opt + os.path.normpath(os.path.join(base_dir, p))
        out.append(f)
    return out


def logical_lines(text):
    """Join backslash-continued lines."""
    buf = ""
    for line in text.splitlines():
        if line.rstrip().endswith("\\"):
            buf += line.rstrip()[:-1] + " "
        else:
            yield buf + line
            buf = ""
    if buf:
        yield buf


def expand(value, local_vars):
    def shell(m):
        cmd = m.group(1).strip()
        out = SHELL_OUTPUT.get(cmd, "")
        # map box-absolute -I paths into the sysroot copy
        return out   # box-absolute -I paths are mapped to the sysroot in makefile_flags()

    value = re.sub(r"\$\(shell\s+([^)]*)\)", shell, value)

    def var(m):
        name = m.group(1) or m.group(2)
        if name in local_vars:
            return local_vars[name]
        return VARS.get(name, "")

    for _ in range(3):   # a little nesting
        value = re.sub(r"\$\(([A-Za-z_][A-Za-z0-9_]*)\)|\$\{([A-Za-z_][A-Za-z0-9_]*)\}", var, value)
    return value


def makefile_flags(mk_path):
    """-I/-D/-U flags from one Makefile, -I resolved against its directory."""
    mk_dir = os.path.dirname(mk_path)
    try:
        text = open(mk_path, errors="replace").read()
    except OSError:
        return None
    if "generated by automake" in text[:400].lower():
        return None
    local_vars, flags = {}, []
    for line in logical_lines(text):
        if line.startswith("\t") or line.lstrip().startswith("#"):
            continue
        m = re.match(r"^\s*(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*(\+|:|\?)?=\s*(.*)$", line)
        if m:
            name, op, val = m.group(1), m.group(2), m.group(3)
            val = expand(val, local_vars)
            local_vars[name] = (local_vars.get(name, "") + " " + val) if op == "+" else val
        if not FLAG_VARS.match(line):
            continue
        val = expand(FLAG_VARS.match(line).group(4), local_vars)
        try:
            toks = shlex.split(val, posix=True)
        except ValueError:
            toks = val.split()
        i = 0
        while i < len(toks):
            t = toks[i]
            if t in ("-I", "-D", "-U", "-include") and i + 1 < len(toks):
                t, i = t + toks[i + 1], i + 1
            if t.startswith("-I"):
                p = t[2:]
                if p and "$" not in p:
                    if os.path.isabs(p) and not p.startswith(ROOT) and not p.startswith(SYSROOT):
                        BOX_REFS.add(os.path.normpath(p))
                        p = SYSROOT + p              # box path (/usr/foo/include) -> local sysroot copy
                    flags.append("-I" + os.path.normpath(os.path.join(mk_dir, p)))
            elif (t.startswith("-D") or t.startswith("-U")) and "$" not in t and len(t) > 2:
                flags.append(t)
            i += 1
    # keep order, drop duplicates
    seen, uniq = set(), []
    for f in flags:
        if f not in seen:
            seen.add(f)
            uniq.append(f)
    return uniq


INCLUDE_RE = re.compile(r'^\s*#\s*include\s*<([^>]+)>', re.M)
HDR_EXT = (".h", ".hpp", ".hh", ".hxx", ".tcc")


def repo_include_dirs(base_flags, kernel):
    """
    Library headers that live in the repo (e.g. system/libs/codeview/codeview.h)
    get installed on the box as <codeview/codeview.h>. Find every
    `#include <dir/file.h>` the tree uses that the base include paths can't
    satisfy, and add the repo folder that can, so clangd reads the repo's
    copy instead of needing one from the box.
    """
    known = [f[2:] for f in base_flags if f.startswith("-I")] + \
            [f[len("-isystem"):] for f in base_flags if f.startswith("-isystem")]

    headers = []          # repo-relative header paths
    wanted = set()        # <a/b.h> spellings used in the tree
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if skipped(dirpath):
            dirnames[:] = []
            continue
        for name in filenames:
            path = os.path.join(dirpath, name)
            if name.endswith(HDR_EXT):
                headers.append(rel(path))
            if is_kernel(dirpath) == kernel and (name.endswith(HDR_EXT) or os.path.splitext(name)[1] in SRC_EXT):
                try:
                    wanted.update(s for s in INCLUDE_RE.findall(open(path, errors="replace").read()) if "/" in s)
                except OSError:
                    pass

    def resolvable(spec, dirs):
        return any(os.path.isfile(os.path.join(d, spec)) for d in dirs)

    # candidate prefix dir -> specs it satisfies
    by_prefix = {}
    unresolved = set()
    for spec in sorted(wanted):
        if resolvable(spec, known):
            continue
        cands = [h[: -len(spec)].rstrip("/") for h in headers if h == spec or h.endswith("/" + spec)]
        if not cands:
            unresolved.add(spec)
            continue
        for c in cands:
            by_prefix.setdefault(c, set()).add(spec)

    # Never let a repo folder shadow libc/libstdc++ (sys/types.h, stdio.h, ...).
    def shadows_system(prefix):
        pdir = os.path.join(ROOT, prefix)
        for h in headers:
            if h.startswith(prefix + "/") if prefix else True:
                r = h[len(prefix) + 1:] if prefix else h
                if resolvable(r, known[1:] if known else []):   # skip the Syllable -I, check sysroot dirs
                    return True
        return False

    chosen, covered = [], set()
    for prefix, specs in sorted(by_prefix.items(), key=lambda kv: (-len(kv[1]), len(kv[0]))):
        new = specs - covered
        if not new or shadows_system(prefix):
            continue
        chosen.append(os.path.join(ROOT, prefix))
        covered |= specs
    leftover = {s for specs in by_prefix.values() for s in specs} - covered
    return chosen, sorted(unresolved | leftover)


def referenced_box_dirs():
    """Box folders (e.g. /usr/freetype/include) that Makefiles add with -I."""
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if skipped(dirpath):
            dirnames[:] = []
            continue
        mk = next((os.path.join(dirpath, n) for n in ("Makefile", "makefile", "GNUmakefile") if n in filenames), None)
        if mk:
            makefile_flags(mk)
    return sorted(BOX_REFS)


def ensure_sysroot(force):
    """Pull .sysroot/ from the box when asked, or when it's missing and a box is configured."""
    import sync_sysroot
    if not force and os.path.isdir(SYSROOT):
        return
    if not sync_sysroot.box_configured():
        if force or not os.path.isdir(SYSROOT):
            print("note: no Syllable box configured (Syllable/.syllable-box), so standard library\n"
                  "      headers (<stdio.h>, <string>, ...) won't resolve. Repo headers still do.\n")
        return
    if sync_sysroot.main() != 0:
        print("note: sysroot sync failed; continuing with what's there.\n")


def main():
    sys.dont_write_bytecode = True   # no __pycache__ in tools/
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    ensure_sysroot(force="--sync" in sys.argv[1:])
    BOX_REFS.clear()

    tools = os.path.dirname(os.path.abspath(__file__))
    base_user = read_base_flags(os.path.join(tools, "base_flags.txt"))
    base_kernel = read_base_flags(os.path.join(tools, "base_flags_kernel.txt"))

    repo_dirs, unresolved = repo_include_dirs(base_user, kernel=False)
    k_dirs, _ = repo_include_dirs(base_kernel, kernel=True)
    base_user = base_user + ["-I" + d for d in repo_dirs]
    base_kernel = base_kernel + ["-I" + d for d in k_dirs]

    entries = []
    mk_cache = {}          # dir -> (flags, dir) of nearest Makefile at or above
    for dirpath, dirnames, filenames in os.walk(ROOT):
        if skipped(dirpath):
            dirnames[:] = []
            continue
        dirnames.sort()

        mk = next((os.path.join(dirpath, n) for n in ("Makefile", "makefile", "GNUmakefile") if n in filenames), None)
        flags = makefile_flags(mk) if mk else None
        if flags is not None:
            mk_cache[dirpath] = (flags, dirpath)
        else:
            parent = os.path.dirname(dirpath)
            mk_cache[dirpath] = mk_cache.get(parent, ([], dirpath))
        mk_flags, mk_dir = mk_cache[dirpath]

        base = base_kernel if is_kernel(dirpath) else base_user

        for name in sorted(filenames):
            ext = os.path.splitext(name)[1]
            lang = SRC_EXT.get(ext)
            if not lang:
                continue
            src = os.path.join(dirpath, name)
            std = "-std=gnu99" if lang == "c" else "-std=gnu++98"
            compiler = "clang" if lang == "c" else "clang++"
            # Makefile -I's go first, then the shared base (its -I/-isystem come after)
            args = [compiler, "-x", lang, std] + mk_flags + base + ["-c", src]
            entries.append({"directory": mk_dir, "file": src, "arguments": args})

    out = os.path.join(ROOT, "compile_commands.json")
    with open(out, "w") as f:
        json.dump(entries, f, indent=1)
    print(f"wrote {len(entries)} entries to {rel(out)}")

    print("\nrepo folders added for <lib/header.h> includes:")
    for d in repo_dirs:
        print("  " + rel(d))
    for d in k_dirs:
        print("  " + rel(d) + "   (kernel only)")

    tops = {}
    for s in unresolved:
        tops.setdefault(s.split("/")[0], []).append(s)
    if tops:
        print("\n<dir/...> includes found in neither the repo nor sysroot (box-only or third-party):")
        for t, specs in sorted(tops.items(), key=lambda kv: -len(kv[1])):
            print(f"  {t}/  ({len(specs)} headers, e.g. {specs[0]})")

    # Box folders the Makefiles reference that aren't in the local sysroot yet.
    missing = sorted({a[2 + len(SYSROOT):] for e in entries for a in e["arguments"]
                      if a.startswith("-I" + SYSROOT + "/") and not os.path.isdir(a[2:])})
    if missing:
        print("\nbox include folders not in sysroot yet (pull them if you work on that code):")
        for m in missing:
            print("  " + m)


if __name__ == "__main__":
    sys.exit(main())
