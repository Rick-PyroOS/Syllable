#!/usr/bin/env python3
"""
Copy the Syllable box's C/C++ library headers into Syllable/.sysroot/ so clangd
can read them on this machine.

Header lookup order for clangd (see tools/base_flags.txt):
  1. system/sys/include          Syllable's own headers, from the repo
  2. system/libs/ (+ other repo  library headers, from the repo
     folders found by tools/gen_compile_commands.py)
  3. .sysroot/                   everything else (glibc 2.7, libstdc++ from the
                                 box's GCC, freetype, ...), copied from the box

The box is named in Syllable/.syllable-box (git-ignored; see
.syllable-box.example) or the SYLLABLE_BOX environment variable. Without one,
tiers 1 and 2 still work; only the standard library headers are missing.

What it does (read-only on the box; it only writes on this machine):
  - asks the box's gcc which folders it searches for headers, and pulls them
  - pulls the box folders the Makefiles add with -I (e.g. /usr/freetype/include)
  - saves the box gcc's predefined macros to .sysroot/gcc-macros.txt
  - rewrites the .sysroot -isystem lines in tools/base_flags*.txt

Usage (from anywhere):
    python3 tools/sync_sysroot.py
You'll be asked for the box's password once (connections are shared). Re-run
after installing libraries on the box. gen_compile_commands.py runs this by
itself when .sysroot/ is missing.
"""

import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # .../Syllable
SYSROOT = os.path.join(ROOT, ".sysroot")
CONFIG = os.path.join(ROOT, ".syllable-box")
FLAG_FILES = {   # base flags file -> whether it gets the C++ library folders
    os.path.join(ROOT, "tools", "base_flags.txt"): True,
    os.path.join(ROOT, "tools", "base_flags_kernel.txt"): False,
}

DEFAULTS = {
    "host": "",
    "rsync_path": "/usr/rsync/bin/rsync",
    # The box runs OpenSSH 5, which only offers ssh-rsa keys. Its host key is kept
    # in its own known_hosts file: trusted on first use, then checked every time.
    "ssh_options": "-o HostKeyAlgorithms=+ssh-rsa -o PubkeyAcceptedAlgorithms=+ssh-rsa "
                   "-o UserKnownHostsFile=~/.ssh/known_hosts_syllable -o StrictHostKeyChecking=accept-new",
    "ssh": "ssh",
}


def load_config():
    cfg = dict(DEFAULTS)
    if os.path.isfile(CONFIG):
        for line in open(CONFIG):
            line = line.split("#", 1)[0].strip()
            if "=" in line:
                k, v = line.split("=", 1)
                cfg[k.strip()] = v.strip()
            elif line and not cfg["host"]:
                cfg["host"] = line          # a bare "root@1.2.3.4" line works too
    if os.environ.get("SYLLABLE_BOX"):
        cfg["host"] = os.environ["SYLLABLE_BOX"]
    return cfg


def box_configured():
    return bool(load_config()["host"])


class Box:
    def __init__(self, cfg):
        self.host = cfg["host"]
        self.rsync_path = cfg["rsync_path"]
        # One shared connection, so the password is asked for once.
        control = ["-o", "ControlMaster=auto", "-o", "ControlPersist=120",
                   "-o", "ControlPath=" + os.path.join(os.path.expanduser("~"), ".ssh", "syllable-box-%r@%h:%p")]
        self.ssh = shlex.split(cfg["ssh"]) + shlex.split(cfg["ssh_options"]) + control

    def run(self, script):
        """Run a shell script on the box in a login shell (so gcc is on PATH)."""
        r = subprocess.run(self.ssh + [self.host, "bash -l"], input=script, text=True,
                           capture_output=True)
        if r.returncode != 0 and not r.stdout:
            sys.exit(f"ssh to {self.host} failed:\n{r.stderr.strip()}")
        return r.stdout

    def pull(self, box_dir):
        """Mirror box_dir to .sysroot/<box_dir>. Returns True on success."""
        dest = SYSROOT + box_dir
        os.makedirs(dest, exist_ok=True)
        cmd = ["rsync", "-a", "--copy-unsafe-links", "--exclude", ".git", "--exclude", ".DS_Store",
               "--rsync-path=" + self.rsync_path, "-e", shlex.join(self.ssh),
               f"{self.host}:{box_dir.rstrip('/')}/", dest.rstrip("/") + "/"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"  ! {box_dir}: {r.stderr.strip().splitlines()[-1] if r.stderr.strip() else 'rsync failed'}")
            return False
        return True


def parse_search_dirs(gcc_v):
    """`gcc -v -E` output -> normalized <...> search folders, in gcc's order."""
    dirs, on = [], False
    for line in gcc_v.splitlines():
        if line.startswith("#include <...> search starts here"):
            on = True
        elif line.startswith("End of search list"):
            break
        elif on and line.startswith(" "):
            d = os.path.normpath(line.strip())
            # gcc's own builtin headers (stddef.h, stdarg.h ...): clang has its own.
            if re.search(r"/lib/gcc/[^/]+/[^/]+/include$", d):
                continue
            if d not in dirs:
                dirs.append(d)
    return dirs


def rewrite_flags(path, dirs, with_cxx):
    """Replace the .sysroot -isystem lines in a base flags file (paths relative to ROOT)."""
    if not os.path.isfile(path):
        return
    base = ROOT
    new = ["-isystem" + os.path.relpath(SYSROOT + d, base) for d in dirs
           if with_cxx or "/c++/" not in d + "/"]
    lines = open(path).read().splitlines()
    out, placed = [], False
    for l in lines:
        if l.startswith("-isystem") and "sysroot" in l:
            if not placed:
                out.extend(new)
                placed = True
            continue
        out.append(l)
    if not placed:
        out.extend(new)
    with open(path, "w") as f:
        f.write("\n".join(out) + "\n")


def main():
    import shutil
    if not shutil.which("rsync"):
        print("rsync isn't installed on this machine.")
        return 1
    cfg = load_config()
    if not cfg["host"]:
        print(f"No Syllable box configured. Put its ssh target (e.g. root@172.16.226.58) in\n"
              f"  {os.path.relpath(CONFIG)}\nor set SYLLABLE_BOX. Repo headers still work without it.")
        return 1
    box = Box(cfg)
    print(f"Asking {box.host}'s gcc for its header folders...")
    out = box.run("gcc -v -E -x c++ /dev/null 2>&1\necho '=== MACROS ==='\ngcc -dM -E -x c /dev/null 2>&1\n")
    gcc_v, _, macros = out.partition("=== MACROS ===\n")
    search = parse_search_dirs(gcc_v)
    if not search:
        sys.exit("Couldn't read gcc's search list from the box. Output was:\n" + out[:2000])

    sys.dont_write_bytecode = True   # no __pycache__ in tools/
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import gen_compile_commands as gen
    # Only include folders: a stray -I/src in some Makefile shouldn't pull a source tree.
    extra = [d for d in gen.referenced_box_dirs() if d not in search and "/include" in d]

    os.makedirs(SYSROOT, exist_ok=True)
    print(f"Pulling into {os.path.relpath(SYSROOT)}/ (read-only on the box):")
    pulled = []
    for d in search + extra:
        parent = next((p for p in pulled if d.startswith(p + "/")), None)
        if parent:                      # e.g. c++/4.1.2/backward, already inside c++/4.1.2
            pulled.append(d)
            continue
        print("  " + d)
        if box.pull(d):
            pulled.append(d)
    with open(os.path.join(SYSROOT, "gcc-macros.txt"), "w") as f:
        f.write("".join(sorted(macros.splitlines(True))))

    good = [d for d in search if d in pulled]
    if good:
        for path, with_cxx in FLAG_FILES.items():
            rewrite_flags(path, good, with_cxx)
        print(f"\nPulled {len(pulled)} of {len(search) + len(extra)} folders; updated tools/base_flags*.txt.")
    else:
        print("\nNothing pulled; base flags left as they were.")
    missing = [d for d in search + extra if d not in pulled]
    if missing:
        print("Not on the box (or unreadable): " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
