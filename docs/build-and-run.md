# Building and running Syllable (Pyro)

This guide takes a machine with nothing on it to a running Syllable you built
yourself. It covers macOS, Linux and Windows (through WSL 2). The same guide
is in both repositories: `Syllable/docs/build-and-run.md` and
`Syllable-Build-System/docs/build-and-run.md`.

Once you are set up, [sb.md](https://github.com/Rick-PyroOS/Syllable-Build-System/blob/main/docs/sb.md) in Syllable-Build-System is the reference
for every `sb` command: building, debugging, packaging and releases.

## How the build works

The build runs in Docker, so the host needs very little. `sb`, a script in
Syllable-Build-System, does everything:

1. `sb setup` builds two Docker images once: `syllable-toolchain-x86` (the
   GCC 4.1.2 and 9.5.0 cross compilers for Syllable) and `syllable-os-x86`
   (that plus CMake, Ninja and the other build tools). It also makes two
   Docker volumes: `syllable-build` (the build tree) and `syllable-golden`
   (the reference disk the image is built from).
2. `sb image` builds Syllable from your checkout and writes a bootable disk,
   `out/syllable.qcow2`.
3. `sb run` boots that disk in QEMU on the host.

The Syllable checkout is only read by the build; nothing is written into it.

## What you need

### Supported hosts

| Host | Docker | Notes |
|---|---|---|
| macOS 13 or newer, Apple Silicon or Intel | Docker Desktop | The original platform. On Apple Silicon, turn on Docker Desktop's "Use Rosetta for x86_64/amd64 emulation" (Settings, General) |
| Linux on x86-64: Debian 12+, Ubuntu 22.04+, Fedora 40+, Arch, openSUSE, Alpine | Docker Engine with the buildx plugin | The fastest host: the containers run natively |
| Windows 10 21H2+ or Windows 11, through WSL 2 | Docker Desktop with WSL integration, or Docker Engine inside WSL | Everything runs inside the WSL Linux distribution. The QEMU window needs WSLg (Windows 11, or a current WSL on Windows 10); without it `sb run` serves VNC |
| Linux on ARM64 | Docker Engine plus amd64 emulation | Works, slowly: the containers are x86-64 |

### Disk, memory, time

- About 40 GB of free disk for Docker (images about 8 GB, volumes about
  15 GB, build cache about 10 GB) plus about 5 GB for the checkouts and
  `out/`.
- 8 GB of memory or more for Docker. In Docker Desktop: Settings, Resources.
- The first `sb setup` builds the cross compilers from source. That takes a
  few hours (longer under emulation). After that, an `sb image` that only
  rebuilds what changed takes minutes.

### The four checkouts

All four go in one folder, next to each other. `sb` finds them there; the
environment variables below can point elsewhere.

| Repository | What it is | Used by |
|---|---|---|
| `Syllable` | The operating system: kernel, drivers, libraries, servers, applications, and the CMake build | Everything |
| `Syllable-Build-System` | `sb`, the Docker images, the release disk (`Syllable/syllable.qcow2.zip`, in Git LFS) | Everything |
| `Syllable-Ports` | Source archives of the ported software (GCC, binutils, glibc, ...) | `sb setup` (the toolchain image) |
| `linux-afs` | The Linux port of Syllable's file system, AFS. See below | `sb image`, `sb release` |

### linux-afs: the Linux AFS port

Syllable's own file system is AFS (the AtheOS File System; not the Andrew
File System). The disk images the build makes are AFS volumes, and Linux
can't read or write AFS on its own. `linux-afs` fills that gap, in two parts:

- **afstool** (`linux-afs/tools/afstool`): Syllable's own AFS driver code,
  compiled unmodified as a Linux program, plus a small shim for the Syllable
  kernel calls. It makes, edits and checks AFS images. **The build needs
  it**: `sb image` and `sb release` build afstool inside the Docker container
  (from the linux-afs checkout, mounted at `/afs`) and use it to create the
  boot volume and copy the built system onto it. Without linux-afs there is
  no disk image. `sb` stops with a message naming the missing folder.
- **sylafs** (`linux-afs/kernel`): a read-only Linux kernel driver, so a Linux
  machine can mount a Syllable disk and look at it (see "Looking inside an
  image" below). The build doesn't need it; it's for inspecting images and
  disks on a Linux host.

## Environment variables

None are required when the four checkouts are side by side. All are optional.

| Variable | Default | What it does |
|---|---|---|
| `SYLLABLE_DIR` | `../Syllable` | The Syllable checkout. Setting it also lets `sb compile` and `sb package` take a program by name, or pick one with fzf |
| `SYLLABLE_BUILD_DIR` | the folder `sb` is in | The Syllable-Build-System checkout |
| `SYLLABLE_PORTS_DIR` | `../Syllable-Ports` | The Syllable-Ports checkout |
| `SYLLABLE_AFS_DIR` | `../linux-afs` | The linux-afs checkout |
| `SYLLABLE_OUT_DIR` | `Syllable-Build-System/out` | Where images, logs and build results go |
| `DOCKER_HOST` | Docker's default | Which Docker to use, when it is not the default one |
| `JOBS` | all CPUs | Parallel jobs for the toolchain build (`JOBS=4 ./sb setup`) |
| `SB_NO_APPS` | unset | `1`: skip the native GCC packages for the image (`apps/`), which take hours more |
| `SYLLABLE_COMPILER` | `4.1.2` | `9.5.0` builds with GCC 9.5.0, in its own build volume |
| `SYLLABLE_DISPLAY` | a window | `vnc`: `sb run` serves VNC on 127.0.0.1:5901 (password `syllable`) instead. The default on Linux without a display |
| `SYLLABLE_AUDIO` | macOS: the speakers; elsewhere none | `wav` records to `out/run-audio.wav`; `none`; or a QEMU audio backend: `pa`, `pipewire`, `alsa` |
| `SYLLABLE_AGENT_PORT` | `2346` | The host port for `sb execute`'s agent inside Syllable |
| `SB_SERIAL` | `on` | `off`: `sb run` doesn't show the kernel log in the terminal |
| `SB_GDB` | the first that works of `i386-elf-gdb`, `gdb-multiarch`, `gdb` | The gdb `sb debug` uses |
| `SB_ADDR2LINE` | found on the PATH | The addr2line `sb symbolize` uses |
| `SB_COLOR` | `auto` | `always` or `never`, for `sb run` and `sb symbolize` |
| `SB_RELEASE_SIZE` | `2G` | The disk size of `sb release` |
| `BOOT_TIMEOUT` | `300` | Seconds `sb boot-test` waits for the boot marker |

## Step 1: Docker and git

Install these first; everything else comes later from a script.

### macOS

1. Install [Docker Desktop](https://docs.docker.com/desktop/setup/install/mac-install/)
   and start it. On Apple Silicon turn on "Use Rosetta for x86_64/amd64
   emulation". Give it 8 GB of memory and 64 GB of disk or more.
2. Install [Homebrew](https://brew.sh), then `brew install git git-lfs`.

### Linux

1. Install Docker Engine from Docker's repository for your distribution:
   <https://docs.docker.com/engine/install/>. That includes the buildx plugin
   `sb` needs. (A distribution's own `docker.io` package works too if you also
   install its buildx package: `docker-buildx` on Debian and Ubuntu, Arch.)
2. Let your user run Docker, then log out and back in:

   ```sh
   sudo usermod -aG docker $USER
   sudo systemctl enable --now docker
   ```

3. Install git with your package manager (`apt install git`, `dnf install git`,
   `pacman -S git`, `zypper install git`, `apk add git bash`).

On an ARM64 machine, also let Docker run x86-64 containers:

```sh
docker run --privileged --rm tonistiigi/binfmt --install amd64
```

### Windows (WSL 2)

1. In PowerShell as administrator: `wsl --install -d Ubuntu-24.04`, then
   restart. On Windows 10, also run `wsl --update` to get WSLg (the QEMU
   window).
2. Docker, one of:
   - [Docker Desktop for Windows](https://docs.docker.com/desktop/features/wsl/)
     with Settings, Resources, WSL integration turned on for Ubuntu-24.04; or
   - Docker Engine installed inside Ubuntu, as for Linux above. This needs
     systemd in WSL: put `[boot]` and `systemd=true` in `/etc/wsl.conf`, then
     `wsl --shutdown` in PowerShell.
3. From here on, work in the Ubuntu shell, and keep everything in WSL's own
   file system (`~/Code/...`), not under `/mnt/c`. On `/mnt/c` the build is
   many times slower and loses Unix file modes.
4. Clone with the git inside WSL, not Git for Windows: a Windows checkout has
   CRLF line endings, which break every script.

## Step 2: check out the four repositories

```sh
mkdir -p ~/Code/Rick-PyroOS && cd ~/Code/Rick-PyroOS
git clone git@github.com:Rick-PyroOS/Syllable.git
git clone git@github.com:Rick-PyroOS/Syllable-Build-System.git
git clone git@github.com:Rick-PyroOS/Syllable-Ports.git
git clone git@github.com:Rick-PyroOS/linux-afs.git
```

The result:

```
~/Code/Rick-PyroOS/
  Syllable/
  Syllable-Build-System/
  Syllable-Ports/
  linux-afs/
```

## Step 3: the host tools

```sh
cd ~/Code/Rick-PyroOS/Syllable-Build-System
tools/install-deps.sh -n    # what it would install, and what it checks
tools/install-deps.sh
```

It works out the platform and its package manager (Homebrew, apt, dnf,
pacman, zypper or apk) and installs QEMU (with its window), git-lfs, unzip,
python3, fzf, a gdb that debugs i386, binutils and mtools. It does not
install Docker; it checks it and says what is wrong (not running, no
permission, no buildx), and it warns about the WSL problems above (a checkout
on `/mnt/c`, CRLF line endings, no display). Run it again any time; it only
installs what is missing.

On RHEL, Rocky or Alma, enable EPEL first (`dnf install epel-release`).
On Alpine, `apk add bash` first.

## Step 4: the release disk

The build starts from the Syllable release disk, which is in Git LFS:

```sh
cd ~/Code/Rick-PyroOS/Syllable-Build-System
git lfs install
git lfs pull
unzip Syllable/syllable.qcow2.zip -d Syllable    # Syllable/syllable.qcow2, 1.9 GB
```

## Step 5: set up the build (once, hours)

```sh
./sb setup -n    # what it is going to build
./sb setup
```

This builds the toolchain image, the native compilers for the image
(`apps/`), the `syllable-os-x86` image, and the two volumes, then configures
the build tree. `JOBS=4 ./sb setup` leaves CPUs free; `SB_NO_APPS=1 ./sb setup`
skips the native compilers. Run `./sb setup` again whenever `toolchain/` or
`os/Dockerfile` changes; it only rebuilds what is out of date.

## Step 6: build and run

```sh
./sb image        # build Syllable and write out/syllable.qcow2
./sb boot-test    # optional: boot it headless and check that it comes up
./sb run          # boot it in a QEMU window
```

Log in as `root`, password `root`. The kernel log shows in the terminal and
is kept in `out/run-serial.log`. Close the QEMU window, or Ctrl-C in the
terminal, to stop.

Under WSL with WSLg, and on a Linux desktop, the QEMU window opens like any
other. With no display (an SSH session, WSL without WSLg), `sb run` serves VNC
instead and says so: connect a VNC viewer to `127.0.0.1:5901` (from Windows
too: WSL forwards localhost), password `syllable`.

## Day to day

Edit in `Syllable/`, then:

```sh
./sb image && ./sb run
```

`sb image` rebuilds and rewrites only what changed. `./sb build TARGET` builds
just one target, `./sb compile PATH` builds a single program and
`./sb execute` runs it in the running Syllable without rebuilding the image.
Debugging the kernel or a program with gdb, crash symbolization, packages and
releases: [sb.md](https://github.com/Rick-PyroOS/Syllable-Build-System/blob/main/docs/sb.md).

## Looking inside an image (Linux)

With linux-afs's kernel driver you can mount a Syllable disk read-only on a
Linux host. Build it against the running kernel (this needs that kernel's
headers: `apt install linux-headers-$(uname -r)`, `dnf install kernel-devel`):

```sh
cd ~/Code/Rick-PyroOS/linux-afs/kernel
make
sudo insmod sylafs.ko

cd ~/Code/Rick-PyroOS/Syllable-Build-System
qemu-img convert -O raw out/syllable.qcow2 out/syllable.raw
sudo losetup -P -f --show out/syllable.raw       # prints /dev/loopN
sudo mkdir -p /mnt/syllable
sudo mount -t sylafs /dev/loopNp1 /mnt/syllable
```

Syllable's file attributes show up as `user.*` extended attributes
(`getfattr -d FILE`). This needs a real Linux kernel: WSL 2's kernel doesn't
take out-of-tree modules without building a custom one, and macOS has no
equivalent. On any host, afstool reads images without mounting them; see
`linux-afs/README.md`.

## Troubleshooting

| What you see | What to do |
|---|---|
| `permission denied ... /var/run/docker.sock` | `sudo usermod -aG docker $USER`, then log out and back in (or `newgrp docker`) |
| `sb: can't reach Docker` | Start Docker Desktop, or `sudo systemctl start docker`. Check `DOCKER_HOST` if you use a Docker other than the default |
| `unknown flag: --build-context` or `buildx` not found | Install the buildx plugin (`docker-buildx-plugin` from Docker's repository) |
| `sb: no linux-afs at ...` | Clone linux-afs next to Syllable, or set `SYLLABLE_AFS_DIR` |
| `syllable.qcow2 is missing` during `sb setup` | Step 4: `git lfs pull` and unzip it |
| `$'\r': command not found`, `bad interpreter` | The checkout has Windows line endings. Clone again from inside WSL |
| No QEMU window under WSL | Update WSL (`wsl --update` in PowerShell), or use VNC: `SYLLABLE_DISPLAY=vnc ./sb run` |
| Files in `out/` owned by root | `sb` gives them back after each Docker run; for leftovers from an older `sb`: `sudo chown -R $USER out` |
| `exec format error` in Docker on an ARM machine | Install amd64 emulation: `docker run --privileged --rm tonistiigi/binfmt --install amd64` |
| `qemu-system-i386: ... gtk` or no window on Linux | Install QEMU's GTK UI: `qemu-system-gui` (Debian, Ubuntu) or `qemu-ui-gtk` (Fedora, Arch, openSUSE, Alpine); `tools/install-deps.sh` does |
| The build is very slow on WSL | The checkout is under `/mnt/c`; move it to `~/Code` |
