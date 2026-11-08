# Init server: audit of today's boot, and a design for its replacement

2026-11-08. Audit and design only. Nothing was changed or built.

- **Source read:** `~/Code/Rick-PyroOS/Syllable` at `e4d8ade4` (branch `feat/docs`). Paths below are relative to that repo unless they say otherwise.
- **Also read:** `origin/feat/input-server` (7e122756), `origin/feat/waiters` (0149578e), `Syllable-Build-System` (`test/overlay-m1`, `release/overlay`, and the latest QEMU serial log, `out/logs/run-serial-20261108-001441.log`).
- **Related docs:** `appserver-review-20261104T120736-ef3a.md` (Section 3, "How to add other servers") and `inputserver-gap-audit-20261102T031604-6c5a.md`.

## TL;DR

1. **There is no supervisor today.** The kernel runs `init`, which forks the appserver and then *becomes* `start.sh`. When the script ends, nothing is left that knows which servers should be running. If the appserver, media server or registrar crashes, it stays dead until reboot.
2. **Start order is decided by races**, not readiness. Everything that is an `os::Application` just blocks in the kernel until the appserver publishes its port. Nothing waits for the registrar or media server.
3. **Several small boot bugs** turned up: init refuses to boot if `/system/early-start.sh` is missing, and the source tree doesn't ship one. The boot mode is passed to `start.sh` and ignored. The kernel's appserver port is never cleared when the appserver dies. Shutdown has no "stop the services" phase.
4. **The design:** a new `init_server` at `/system/programs/init` (the path the kernel already runs). It is a Syllable message server built on `os::Looper` with a public port, `l:init_server`, not on `os::Application`, so it runs before the appserver and survives its death. It reads one small service file per server, starts them in dependency order, waits for each to say it's ready, watches them with `SIGCHLD`/`wait4`, restarts them with backoff, and runs an ordered shutdown. Clients talk to it with `os::Message`s, through a small `os::ServiceManager` class in libsyllable, the same way they use `RegistrarManager`.
5. **Reuse what exists.** `feat/input-server` already defines `M_SERVER_PING`, `M_SERVER_RESTART`, `M_SERVER_QUIT`, `M_SERVER_LOAD/SAVE_*_CONFIG`, `M_SYSTEM_LOGIN` and `M_SYSTEM_SHUTDOWN` in `messages.h`. The design uses them as the server control protocol and adds only `M_SERVER_READY` and the init requests.
6. **Every server is built on one base library, `libserver`** (Section 2.8): the init handshake, the server control messages, config files, logging, client tracking and a common `main()`, plus a matching base for client-side manager classes.
7. **A Services app** (Section 2.13) lists every server and Unix daemon with live state, and starts, stops, restarts, enables and edits them, with a `service` command that does the same from a shell.
8. **First milestone** is a boot that is equivalent to today's, with `start.sh` replaced by service files, and the existing `sb` boot test still passing. Restarts, shutdown, logging and the new servers come after that (Part 3).

---

# Part 1: Audit

## 1.1 The boot chain today

```
GRUB  menu.lst: kernel /system/kernel.so root=... bootmode=... appserver=...
  │
kernel  system_init()                         system/sys/kernel/kernel/init.c:478
  ├─ Fork("init")  parent → idle loop
  │   └─ kernel_init(): scheduler, VFS, boot FS mount, busmanagers, drivers
  │      This kernel thread is thread 1 ("psycho_killer"). It ends in
  │      for(;;) sys_wait4(-1) and reaps orphans.         init.c:589-593
  └─ Fork("sh-host")                                       init.c:513
      ├─ reads a pipe and copies it to the kernel debug log   init.c:571-586
      └─ Fork("boot-shell"): setsid, stdin=/dev/null,
         stdout/stderr = that pipe, DLL_PATH set
         execve("/boot/system/programs/init", "init" [-p appserver] bootmode)
                                                            init.c:520-565
            │
/system/programs/init                 system/apps/utils/systools/init.c (69 lines)
  ├─ symlinks /system /bin /usr /resources → boot/...      :46-49
  ├─ system("/system/early-start.sh"), exit(FAILURE) if it fails  :51-54
  ├─ symlinks /etc /home /var /tmp                         :56-59
  ├─ fork → execl(/system/appserver)                       :61-64
  └─ execl("/system/start.sh", "start.sh", bootmode)      :66
            │
/system/start.sh                      system/image/system/start.sh
  ├─ dbterm 8 >> /var/log/kernel &                         :16
  ├─ ln -s boot/applications boot/documentation /          :19-20
  ├─ source /etc/profile; empty $TEMP                      :24-30
  ├─ no dlogin? → aterm &; exit                            :34-38
  ├─ dlogin  >> /var/log/desktop &                         :41
  ├─ mediaserver > /dev/null &                             :46
  ├─ registrar   > /dev/null &                             :47
  ├─ Network --detect     (GUI; blocks on a dialog when the NIC changed)  :50
  ├─ source network-start.sh   (dhcpc)                     :52
  ├─ inetd & (if installed)                                :56-59
  ├─ "Audio & Video" --setupdefaults & (first boot)        :62-66
  ├─ source every /system/index/tasks/start/*              :77-80
  │     cupsd, gdbserver, sb-agent, sshd (each checks /system/config/<name>)
  ├─ source every /resources/index/tasks/start/*           :82-85
  └─ source user-start.sh; the script exits                :88
```

`system/other/init/` has an older copy of the same scripts, but `system/other/CMakeLists.txt` doesn't install it. The installed copy is `system/image/system/`. The two differ only in the media-defaults block.

The serial log from the latest `sb run` confirms this order: `boot-shell : Starting init...`, then the overlay's `early-start.sh` markers, then `appserver : Appserver Alive`, then dhcpc, the media server and `Registrar running at port 1`.

## 1.2 Who starts what, and how it is found

| Process | Started by | How clients find it | Restarted today? | Output goes to |
|---|---|---|---|---|
| appserver | `init` (fork) | Kernel global, `set_app_server_port()` / `get_app_server_port()` (`kernel/kernel/kernel.c:65-90`) | No | Kernel log (`dbprintf`) |
| dlogin | `start.sh &` | `os::Application` "application/x-vnd.syllable-login_application" | No. It loops itself: login → fork desktop as user → wait → login again (`apps/utils/dlogin/commonfuncs.cpp:120-150`) | `/var/log/desktop` |
| mediaserver | `start.sh &` | `find_port("l:media_server")`, public since `mediaserver.cpp:989` | Only if `Run()` returns: `while(1){ new MediaServer; Start(); }` (`:1000-1007`). A crash isn't covered | `/dev/null` |
| registrar | `start.sh &` | `find_port("l:registrar")` (`registrar.cpp:1524`) | Same `while(1)` (`:1535-1542`), and `OkToQuit()` is false | `/dev/null` |
| dbterm | `start.sh &` | n/a; copies debug port 8 to a file | No | `/var/log/kernel` |
| dhcpc, inetd, cupsd, sshd, gdbserver, sb-agent | `start.sh` and tasks/start scripts | TCP ports | No | Their own files in `/var/log` |

## 1.3 Problems found

1. **No supervisor after boot.** `init` replaces itself with `start.sh` (`init.c:66`), and the script ends. Children are reparented to kernel thread 1 (`kernel/kernel/fork.c:198`), which only reaps them (`kernel/kernel/init.c:592`). Nothing restarts a crashed server or knows the system's intended state.

2. **Start order is a race.** Every `os::Application` calls `get_app_server_port()` in its constructor (`libsyllable/util/application.cpp:73`). The user-level syscall (`sys_get_app_server_port`) spins in `snooze(1000)` until the appserver has published its port (`kernel.c:78-85`), so GUI servers "wait" by accident. The appserver publishes only after it has loaded graphics and input drivers (`server/server.cpp:859-866`). Nothing waits for the registrar or media server. A client that runs too early gets `find_port() < 0` and has to cope.

3. **The appserver port is never cleared.** `g_hDisplayServerPort` is set once and never reset when the port dies (`kernel.c:65-69`). After an appserver crash, new `Application`s don't wait for a new appserver. They get a dead port, and the constructor's `assert( m->m_hSrvAppPort >= 0 )` fires (`application.cpp:100`). `sys_set_app_server_port()` also has no permission check, so any process can take over the role.

4. **Process-death notification goes only to the appserver, and it can block.** When a process's last thread exits, the kernel sends a `-1` message to the appserver's port with `INFINITE_TIMEOUT` (`fork.c:133`, `:168-171`). If the appserver is wedged with a full port, every exiting process hangs in exit. The registrar learns about dead clients only because the appserver forwards that message (`server/server.cpp:266-277`).

5. **`early-start.sh` is required but not shipped.** `init` exits with failure if `system("/system/early-start.sh")` fails, and a missing file fails (`init.c:51-54`). The source tree has none. `sb` boots only because it copies one in from `Syllable-Build-System/test/overlay-m1/system/early-start.sh` (dev images) or `release/overlay/system/early-start.sh` (releases, just `exit 0`). An image built any other way stops after the kernel prints "Starting init...", with no message on screen.

6. **The boot mode is ignored.** The kernel accepts `bootmode=` (`kernel/kernel/init.c:686`) and passes it to `init`, which passes it to `start.sh` as `$1` (`init.c:66`). Nothing in `start.sh` reads `$1`. There is no text-only, safe or rescue mode.

7. **Shutdown has no service phase.** `shutdown` and the Dock send `DR_CLOSE_WINDOWS` to the appserver (`apps/utils/systools/shutdown.cpp`). `AppServer::CloseWindows()` quits every app except four hard-coded names (`server/server.cpp:1043-1046`), closes the Desktop, then calls `apm_poweroff()` or `reboot()` directly (`:1081-1082`). The kernel then sends `SIGTERM` to every thread, waits 2 s, sends `SIGKILL`, flushes the block cache and unmounts (`kernel.c:568-610`). The media server and registrar get no chance to save in order. The registrar saves a user's database only on logout (see the appserver review, Section 2).

8. **Logging is scattered.** Kernel messages go to the debug port and `dbterm 8` copies port 8 to `/var/log/kernel`. The appserver's and init's stdout go through the `sh-host` pipe into the kernel log. dlogin writes `/var/log/desktop`, and the media server and registrar write to `/dev/null`. Each daemon has its own file. inetutils' `syslogd` is installed (`/usr/inetutils/1.6`) but never started, and I found no `AF_UNIX` in the kernel's socket layer (`kernel/kernel/net/`), so glibc's `syslog()` (which writes to `/dev/log`) probably has nowhere to go. That last point is from reading, not from a test.

9. **In-process restart loops hide failures.** `while(1){ new Server; Start(); }` in the registrar and media server restarts only after a clean return and leaks the old object. It also means a supervisor can't tell "exited on purpose" from "restarted". The screensaver waiter on `feat/waiters` copies the same pattern (`system/sys/waiters/screensaver/main.cpp`).

10. **A GUI dialog sits in the boot path.** `Network --detect` runs in the foreground of `start.sh` and blocks it when the NIC has changed (see `system/image/README.md`). Everything after it, including gdbserver and sshd, waits for someone to click OK.

11. **dlogin is both the login screen and the session manager.** It runs as root, checks the password, and forks the desktop with `setuid()` (`commonfuncs.cpp:120-150`). There's no separate place for authentication or session state. This is what loginserver should take over.

12. **Everything runs as root**, and there is no notion of a per-service user. This doesn't block the design, but the service file should have a `user` field from the start.

Already in the tree and worth reusing:
- `feat/input-server`'s `messages.h` adds `M_SYSTEM_LOGIN = 2000000000`, `M_SYSTEM_SHUTDOWN`, `M_SERVER_PING`, `M_SERVER_RESTART`, `M_SERVER_QUIT`, `M_SERVER_LOAD_DEFAULT_CONFIG`, `M_SERVER_SAVE_DEFAULT_CONFIG`, `M_SERVER_LOAD_USER_CONFIG`, `M_SERVER_SAVE_USER_CONFIG`. No code handles them yet. They're Dee's server control protocol, and this design adopts them.
- `origin/feat/waiters` has the screensaver waiter (an `Engine` plus a `ScreenSaver` plugin class in `include/waiters/screensaver.h`). It's the start of waiterserver.
- `feat/input-server`'s `kernel/kernel/services.c` is a *kernel* module registry (like busmanagers), not a process supervisor, and it isn't built. It doesn't overlap with this design.
- The appserver's event bus (`os::Event`, `server/event.cpp`) is the established way to broadcast state. It lives inside the appserver, so init can't depend on it, but it can mirror into it (Section 2.8).

---

# Part 2: Design

## 2.1 Goals

- One long-lived process, the parent of every system server, that knows what should be running and keeps it running.
- Syllable style: a message server with a public port, `os::Message` requests and replies, a client class in libsyllable, change notification by message. No sockets and no text protocol.
- Boots with or without graphics (a text or rescue boot mode).
- Small and hand-editable: one plain-text file per service, in the same index layout packages use today.
- An ordered shutdown that lets servers save state.
- Incremental: the first version reproduces today's boot exactly.

Not goals for the first version: parallel boot, cgroup-like resource limits, socket activation, or any security model beyond running a service as a named user.

## 2.2 Shape of the process

- **Binary:** `/system/programs/init`, the path the kernel hard-codes (`kernel/kernel/init.c:565`). Source in `system/sys/servers/init/`, next to where the input server is going (`system/sys/servers/`).
- **Name and port:** `init_server`, public port `l:init_server`.
- **Base class:** `os::Looper` plus `make_port_public()`, **not** `os::Application`. `Application`'s constructor needs the appserver (Section 1.3, item 2), and init must start before the appserver and outlive it. Init uses only the non-GUI parts of libsyllable (`Looper`, `Message`, `Messenger`, `String`). If loading libsyllable that early turns out to be a problem, the fallback is plain ports and flattened messages, as the kernel side already does.
- **Threads:**
  - the Looper thread handles requests and owns all state;
  - a reaper thread waits in `wait4(-1, ..., 0)` and posts `INIT_CHILD_EXITED` (pid, status) to the Looper. Init is the parent of every service, so this is reliable, and it doesn't depend on the appserver's `-1` message;
  - a timer (Looper timers or a thread) handles restart backoff, start timeouts and optional pings.
- **Never exits.** If the Looper itself throws, `main()` logs it and drops to a rescue shell on the serial port instead of returning. A dead init today means a hung machine with no message.

Init keeps doing what `init.c` does now before starting anything: the root symlinks, then `early-start.sh` **only if it exists** (fixes item 5), then the second set of symlinks.

## 2.3 Service files

One file per service, `key = value`, `#` comments. System services live in `/system/config/services/`. Packages add theirs to `/resources/index/services/`, the same way they add `tasks/start/` scripts today, so the package manager's indexing keeps working. A file in `/system/config/services/local/` with the same name overrides a shipped one, so local edits survive upgrades.

Fields:

| Key | Meaning | Default |
|---|---|---|
| `exec` | Program and arguments | required |
| `description` | One line for `service list` | empty |
| `requires` | Services that must be ready first. If one stops, this one stops too, and it restarts when that one is back | none |
| `wants` | Started first if present, but its failure doesn't stop this one | none |
| `ready` | How init knows it's up: `message` (the service sends `M_SERVER_READY`), `port:<name>` (init polls `find_port`), or `started` (as soon as it's running) | `message` |
| `ready_timeout` | Seconds before a start counts as failed | 15 |
| `restart` | `always`, `on-failure` or `never` | `on-failure` |
| `restart_delay` | First delay in seconds; it doubles each time, up to 30 s | 1 |
| `restart_limit` | `N/seconds`. After N restarts in that window the service goes to `failed` and stays down until someone starts it | `5/60` |
| `critical` | `true`: if it fails for good, go to the rescue target and print why | false |
| `user` | Run as this user | root |
| `log` | `syslog`, `file:<path>` or `none` | `syslog` |
| `boot` | Boot modes it runs in: `normal`, `text`, `rescue` | `normal` |
| `stop` | `message` (send `M_SERVER_QUIT`), or `signal` (`SIGTERM`) | `message` |
| `stop_timeout` | Seconds before `SIGKILL` | 5 |
| `ping` | Seconds between `M_SERVER_PING`s, 0 for off. Three missed replies count as a hang, and init restarts the service | 0 |
| `control` | Who may stop, restart or disable it through init: `system`, `admin` or `user` (Section 2.18) | `admin` |
| `enabled` | `false` keeps the file but doesn't start it. Replaces the `/system/config/sshd`-style "on/off" files | true |

Example, the media server today:

```
# /system/config/services/mediaserver
description = Audio and video mixing and routing
exec        = /system/mediaserver
requires    = appserver
ready       = port:l:media_server
restart     = always
```

## 2.4 Boot targets

The kernel's `bootmode=` value picks a target. Init starts every enabled service whose `boot` list contains it, in dependency order.

- `normal`: everything (today's boot).
- `text`: no appserver and nothing that requires it. Init starts a shell on the serial port and the console.
- `rescue`: init plus syslogserver and a shell. The target init falls back to when a `critical` service fails.

The GRUB menu gets entries for the text and rescue modes.

## 2.5 Lifecycle

States: `stopped → starting → ready → stopping → stopped`, plus `failed` and `waiting` (for a dependency).

- **Start:** fork, set the user and environment (the `/etc/profile` values init has read, plus `INIT_SERVICE=<name>`), connect stdout and stderr to a log pipe (Section 2.9), exec. The state is `starting` until the readiness condition is met or `ready_timeout` passes.
- **Ready:** the service sends `M_SERVER_READY` to `l:init_server`, with its port and pid. That's the preferred way because the service knows when it can answer. `port:` polling covers servers that haven't been changed, such as today's registrar and media server.
- **Exit:** the reaper reports the pid. Init logs the exit status, applies the restart policy, and stops anything that `requires` it.
- **Restart:** wait (backoff), then start again. A dependency that comes back up restarts the services that were stopped because of it.

The **appserver** is the important case. When it dies, every GUI client loses its connection, so all services that `require` it stop. Init then:
1. calls `set_app_server_port(-1)`, so new `Application`s block until the new appserver is up instead of getting a dead port (this works around item 3 until the kernel fixes it);
2. restarts the appserver and waits for `M_SERVER_READY`;
3. restarts its dependents in order: inputserver, then mediaserver, clipboardserver, waiterserver, then loginserver, which shows the login screen again.

The user's session is lost. That's true today as well, and keeping apps alive across an appserver restart is a separate, much larger project.

### How start and stop order is kept

Order comes only from `requires` and `wants` in the service files. There are no numbers, run levels or file-name prefixes.

1. **Build the graph.** At boot, and on `INIT_RELOAD`, init reads every enabled service file for the current boot target and makes a graph: an edge from A to B for each `requires = B` or `wants = B` in A's file. A `requires` that names a missing service marks A `failed` with a clear reason. A missing `wants` is ignored.
2. **Reject cycles.** Init sorts the graph topologically. If there's a cycle, it logs the services in it, marks them `failed`, and boots the rest. A cycle can't hang the boot.
3. **Start a service only when everything before it is ready.** "Ready" means the service sent `M_SERVER_READY`, or its `ready` condition was met (Section 2.3), not just that the process exists. Services whose dependencies are all ready start at the same time. So syslogserver goes first, then the appserver, translation_server and unixserver together, and then inputserver, mediaserver, registrar, clipboardserver and waiterserver once the appserver is ready. loginserver goes once the registrar is ready.
4. **Deal with failure.** If a `requires` dependency fails for good, its dependents stay in `waiting` and the reason shows in `service status`. If a `wants` dependency fails, its dependents start anyway.
5. **Stop in reverse.** Shutdown and `service stop` stop dependents first and wait for each to exit (or for `stop_timeout`) before stopping what they depend on. syslogserver is last.
6. **Restart dependents.** If a service restarts, everything that `requires` it is stopped in reverse order and started again in forward order once it's ready again (Section 2.5).

unixserver and loginserver use the same code from libserver for their own tiers. unixserver builds its graph from the init.d scripts' LSB headers (`Required-Start` and `Should-Start` play the parts of `requires` and `wants`). loginserver builds its graph from the session service files. The three tiers are ordered against each other through init: unixserver and loginserver are ordinary init services, and their sub-services only start once they are ready.

`service order` prints the computed start order, and the Services app's Dependencies tab (Section 2.13) shows the same graph.

## 2.6 Dependency graph

```
syslogserver ─┬─ appserver ─┬─ inputserver
              │             ├─ mediaserver
              │             ├─ clipboardserver
              │             ├─ registrar
              │             ├─ waiterserver   (wants inputserver)
              │             └─ loginserver    (requires registrar, wants inputserver)
              ├─ translation_server   (no appserver needed)
              └─ unixserver   (network, inetd, sshd, cupsd, package tasks)
```

Everything `wants` syslogserver, so logging is up first, but nothing fails if it isn't. unixserver doesn't need graphics, so it starts at the same time as the appserver, and a slow DHCP no longer holds up the login screen.

## 2.7 The message protocol

A new public header, `include/syllable/servers/init.h` (or wherever the input server's headers settle), holds the codes. The server-side `M_SERVER_*` and `M_SYSTEM_*` codes stay in `messages.h`, where `feat/input-server` already put them.

**Requests to `l:init_server`** (all reply with `error` (int32) and the fields shown):

| Code | Fields in | Reply fields |
|---|---|---|
| `INIT_LIST_SERVICES` | none | `name`[], `state`[], `pid`[] |
| `INIT_GET_SERVICE` | `name` | `state`, `pid`, `port`, `restarts`, `last_exit`, `description` |
| `INIT_START_SERVICE` / `INIT_STOP_SERVICE` / `INIT_RESTART_SERVICE` | `name` | none |
| `INIT_ENABLE_SERVICE` / `INIT_DISABLE_SERVICE` | `name` | none (writes the override file) |
| `INIT_RELOAD` | none | `added`[], `removed`[] (rereads service files) |
| `INIT_WATCH` / `INIT_UNWATCH` | `port`, optional `name` | none. The watcher gets `INIT_SERVICE_CHANGED` messages |
| `M_SERVER_READY` | `name`, `port`, `pid` | none. Sent by a service |
| `M_SYSTEM_SHUTDOWN` | `action`: 0 logout, 1 power off, 2 reboot (the numbers `shutdown.cpp` and the Dock already use) | none |

**Messages from init to a service** (each service handles them on its main port):

| Code | Meaning |
|---|---|
| `M_SERVER_PING` | Reply at once. Only sent if the service file has `ping` |
| `M_SERVER_QUIT` | Save state and exit. Init sends `SIGKILL` after `stop_timeout` |
| `M_SERVER_RESTART` | Optional. Restart in place if the server can do it cheaply, otherwise exit and init restarts it |
| `M_SERVER_LOAD_USER_CONFIG` / `M_SERVER_SAVE_USER_CONFIG` | Sent by loginserver on login and logout, with `user` and `home` |

**Notifications:** `INIT_SERVICE_CHANGED` (`name`, `old_state`, `new_state`, `pid`) goes to every watcher's port. That works with no appserver. When the appserver is up, init also posts the same message to an `os::Event` called `os/Init/ServiceChanged`, so Dock plugins and preference apps can use the normal event API. When the appserver isn't up, init simply skips that step.

**Client class:** `os::ServiceManager` in libsyllable (`storage/` or `util/`, next to `RegistrarManager`), reference counted with `Get()`/`Put()` like `RegistrarManager`:

```cpp
os::ServiceManager* pcInit = os::ServiceManager::Get();
std::vector<os::ServiceInfo> cList = pcInit->ListServices();
pcInit->Restart( "mediaserver" );
pcInit->Watch( pcMyHandler, "appserver" );   // INIT_SERVICE_CHANGED to that handler
pcInit->Shutdown( os::ServiceManager::REBOOT );
pcInit->Put();
```

A command-line tool, `/system/programs/service` (`service list`, `service status mediaserver`, `service restart mediaserver`), wraps it. It's the first client to write, and it's what the boot test uses.

## 2.8 libserver: the base library every server is built on

Rick asked on 2026-11-08 for one base library that all servers are built on. Without one, each new server copies the registrar's pattern (appserver review, Section 3.1), including its `while(1)`, its own port lookup and its own way of noticing dead clients. `libserver` puts all of that in one place, and init uses the same library for the parts it shares with servers: the `key = value` parser and the protocol header.

### Where it lives

- Source: `system/sys/servers/libserver/`, next to the servers.
- Installed as `/system/libraries/libserver.so.1`, with headers in `/system/development/headers/server/`.
- Depends on libsyllable only. It has no GUI code, so servers that don't need the appserver don't pull it in.
- A CMake helper, `syllable_server( name SOURCES ... SERVICE <file> )`, links libserver, installs the binary in `/system/servers/` (the existing servers keep their current paths, such as `/system/mediaserver`, until there's a reason to move them) and installs its service file in `/system/config/services/`. Adding a server is then a directory, a CMakeLists.txt with one call, and a service file.

### What's in it

| Header | Class | Job |
|---|---|---|
| `server/protocol.h` | (codes only) | The `M_SERVER_*` codes (moved here from `messages.h`, or included from there), `M_SERVER_READY`, and the `INIT_*` requests and replies. The only place these numbers are defined |
| `server/server.h` | `os::Server` | Base class for servers that don't need the appserver: syslogserver, clipboardserver, unixserver, loginserver's logic. An `os::Looper` with a public port |
| `server/guiserver.h` | `os::GuiServer` | The same for servers that are appserver clients: inputserver, mediaserver, registrar, waiterserver. An `os::Application` |
| `server/servercore.h` | `os::ServerCore` | The shared code both of those delegate to, so the behaviour is identical whichever one a server uses |
| `server/config.h` | `os::ServerConfig` | Reads and writes `key = value` files (Section 2.3). init, every server and the `service` tool use this one parser |
| `server/log.h` | `os::ServerLog` | `Log( LOG_INFO, "..." )`. Sends to `l:syslog_server` when it's up, otherwise writes to stderr, which init's pipe catches (Section 2.9) |
| `server/clients.h` | `os::ClientList` | Tracks client processes and calls `ClientDied()` when one goes away |
| `server/serverlink.h` | `os::ServerLink` | The base for client-side manager classes (Section 2.7) |

`os::ServerCore` exists because `os::Application` is already a `Looper`, so `GuiServer` can't inherit from `Server` without a diamond. Both thin classes forward `HandleMessage()` to the core first, and the core handles the `M_SERVER_*` messages before the server's own code sees anything.

### The server API

```cpp
class MediaServer : public os::GuiServer          // or os::Server
{
public:
    MediaServer() : GuiServer( "media_server" ) {}   // port l:media_server

    // Lifecycle hooks. Each has a default that does nothing and succeeds.
    status_t Init();                       // open devices, load config. An error means exit, and init restarts per policy
    void     SaveState();                  // on M_SERVER_QUIT and before a restart
    void     LoadUserConfig( const os::String& cUser, const os::String& cHome );
    void     SaveUserConfig( const os::String& cUser, const os::String& cHome );
    void     ClientDied( proc_id hProcess );

    void     HandleMessage( os::Message* pcMsg );    // the server's own protocol
};

SERVER_MAIN( MediaServer )
```

`SERVER_MAIN` writes `main()` once for every server:
- it creates the server, calls `Init()`, and if that succeeds makes the port public and calls `Ready()`, which sends `M_SERVER_READY` (name, port, pid) to `l:init_server`;
- it runs the Looper, catches exceptions and turns them into a logged message and a non-zero exit code, so init sees a failure and not a hang;
- it never loops. Init does the restarting;
- `--standalone` skips the init handshake, so you can run a server by hand under gdb or from `sb execute` while you work on it. `--config <file>` points it at a different config file.

`ServerCore` handles, with no code in the server:
- `M_SERVER_PING`: replies at once from the Looper thread, so a server stuck in `HandleMessage()` shows up as a hang;
- `M_SERVER_QUIT`: `SaveState()`, then quit the Looper, then exit 0;
- `M_SERVER_RESTART`: `SaveState()`, then exit with a code that tells init "restart me now, without backoff";
- `M_SERVER_LOAD/SAVE_DEFAULT_CONFIG`: rereads or writes `/system/config/<name>` with `ServerConfig`;
- `M_SERVER_LOAD/SAVE_USER_CONFIG`: calls the hooks above with `user` and `home`. loginserver sends these at login and logout;
- `M_SYSTEM_LOGIN` and `M_SYSTEM_SHUTDOWN` broadcasts: available as optional virtuals for servers that care.

`GuiServer` also passes the `APP_SERVER` flag to `Application`, so the appserver doesn't close it at logout (Section 2.10). That replaces the four hard-coded names in `CloseWindows()` (`server/server.cpp:1043-1046`).

### Client tracking

`ClientList` is how a server learns that a client process died, which every server needs (the registrar does it today with the appserver's forwarded `-1` hack, Section 1.3 item 4). It uses, in order:
1. `os/System/ProcessHasQuit` from the appserver's event bus, when the appserver is up;
2. otherwise, a check every few seconds that each client's process still exists (`get_process_info()`), so non-GUI servers such as syslogserver also work in text mode.

Kernel change 4 in Section 2.12 would replace both with a direct notification.

### The client side

`ServerLink` is the base for the manager classes programs use, so they all behave the same way:
- `Get()` and `Put()` reference counting, as `RegistrarManager` does today;
- finds `l:<name>`. If the server isn't up yet, it waits with a timeout, and it can ask init whether the service is `starting` or `failed`, so the error is clear;
- reconnects after the server restarts. It watches the service through `INIT_WATCH` and re-sends its login message (for example `REGISTRAR_LOGIN`) when the server is back;
- `SendRequest()` with a timeout, so a dead server never hangs a client forever.

`os::ServiceManager` (init's own client, Section 2.7), `RegistrarManager`, the media manager and the new clipboard client are built on it. The existing classes keep their public APIs, so applications don't change.

### The order to build it

- **With M1:** `server/protocol.h` and `ServerConfig`, because init needs them first.
- **With M3:** `ServerCore`, `Server`, `GuiServer`, `SERVER_MAIN`, `ServerLog`, plus a small example server in `system/sys/servers/example/` that the boot test starts, pings, restarts and stops.
- **With M6:** `ClientList` and `ServerLink`, and each existing server moved onto the library as it's touched: the media server and registrar first, because they're the simplest.

## 2.9 Logging

Every line a service writes ends up in one place, tagged with where it came from, without the service having to do anything special.

### Where the lines come from

1. **stdout and stderr.** Init gives every service a pipe for each, and reads them on one reader thread for all services (using `select()` if it works on pipes here, otherwise one thread per service). stdout lines are logged at `info` and stderr lines at `warning`. Nothing is lost if a server just uses `printf()` or `dbprintf()`-style output to stderr.
2. **libserver's `ServerLog`.** Servers built on libserver call `Log( LOG_ERR, "can't open %s", ... )`. That sends a `SYSLOG_WRITE` message straight to syslogserver with a level and optional fields, and falls back to stderr (and so to init's pipe) if syslogserver isn't up.
3. **glibc `syslog()`.** Ported daemons such as sshd, cupsd and inetd call `syslog()`, which normally writes to the `/dev/log` socket. There's no `AF_UNIX` in the kernel's socket layer (Section 1.3, item 8), so the libc `syslog()` gets a small Syllable version that sends `SYSLOG_WRITE` to `l:syslog_server`. Ported daemons need no source changes.
4. **The kernel log.** syslogserver takes over `dbterm 8`'s job and reads the kernel's debug output, tagged `kernel`.
5. **unixserver's init.d scripts and loginserver's session services.** They give their children pipes the same way init does and forward the lines tagged `unix/<script>` and `session/<name>`.

### What a record holds

Each record has a timestamp (`get_real_time()`, microseconds), a level, a tag (the service name, such as `mediaserver`, `unix/sshd` or `session/dock`), the pid, the uid, and the text. The levels are syslog's eight: emerg, alert, crit, err, warning, notice, info and debug. A `SYSLOG_WRITE` may also carry extra named fields, kept as they are.

The tag and pid come from init's records for the pipe, not from the service, so one service can't log as another. `SYSLOG_WRITE` messages over the port carry the tag the sender claims, so until the kernel tells receivers who sent a message (Section 2.18), syslogserver marks those records with the sender's claimed tag and checks it against init's list of service pids where it can.

### Where they go

syslogserver is a system server (`l:syslog_server`), the first one init starts. It writes:
- `/var/log/system.log`: everything, one line per record, in a fixed format: `2026-11-08 15:41:53.120 mediaserver[23] warning: no audio device found`;
- `/var/log/services/<tag>.log`: one file per service, for reading one service's history quickly;
- `/var/log/kernel.log`: the kernel log, as `/var/log/kernel` is today.

Files rotate by size: 1 MB each, five old copies kept (`system.log.1` to `system.log.5`). The sizes and counts are set in `/system/config/syslogserver` (`key = value`, like everything else). It calls `sync()` after crit or worse, so the last lines before a crash are on disk.

### Before syslogserver is up, and if it dies

Init keeps the last 64 KB of each service's output in memory whatever happens, and also writes it to the kernel log the way `sh-host` does now. So the very first lines of the boot, and anything written while syslogserver is restarting, still reach the serial port (and `sb run`'s serial log). When syslogserver comes back, init sends it what it buffered in the meantime, so the files have no gap. The boot test can keep reading the serial port.

### Reading logs

- `service log mediaserver` (the last lines, from init's buffer or the file) and `service log -f mediaserver` (follow).
- `syslog` or `log` for the combined log, with filters: `log --tag unix/sshd --level warning --since 10m`.
- The Services app's Log tab (Section 2.13), for one service, live.
- A Log viewer app that reads `/var/log/system.log` with the same filters.

User servers log to the same syslogserver, tagged `session/<name>` with their uid. A user can read the records for their own uid. Reading everyone's needs root once users are separate (Section 2.18).

## 2.10 The servers

| # | Server | Exists? | Base | Requires | Ready | Notes |
|---|---|---|---|---|---|---|
| 1 | appserver | Yes | its own loop | (wants syslogserver) | `message`: one line after `set_app_server_port()` (`server/server.cpp:866`) | `critical`, `boot = normal`. When it restarts, its dependents restart (Section 2.5) |
| 2 | inputserver | On `feat/input-server`, not compiling yet | `GuiServer` | appserver | `message` | Feeds the appserver through the bridge plugin in appserver review 3.3. The appserver keeps its built-in input plugins as a fallback, so a crashed inputserver loses no input while init restarts it |
| 3 | mediaserver | Yes | `GuiServer` | appserver | `port:l:media_server` now, `message` later | Drop the `while(1)`. Save the mixer state on `M_SERVER_QUIT` |
| 4 | syslogserver | No | `Server` | none | `message` | Starts first. Needs the libc `syslog()` change (2.9) |
| 5 | loginserver | No; today's dlogin does it | `Server` (logic) plus dlogin as its UI client | appserver, registrar | `message` | Owns authentication, sessions and `BecomeUser()`. Broadcasts `M_SYSTEM_LOGIN`, sends `M_SERVER_LOAD/SAVE_USER_CONFIG` to the other servers, and handles logout (`M_SYSTEM_SHUTDOWN` with action 0). dlogin becomes the login window only |
| 6 | clipboardserver | No; inside the appserver today (`DR_SET_CLIPBOARD_DATA`, `server/server.cpp:932`, event `os/System/ClipboardHasChanged`, `:211`) | `Server` | appserver (for the event bus only) | `message` | `os::Clipboard` talks to the appserver port today (`libsyllable/util/clipboard.cpp:49-50`). Point it at `l:clipboard_server`, and keep the appserver code as a fallback until it works |
| 7 | waiterserver | Screensaver waiter on `origin/feat/waiters` | `GuiServer` | appserver, wants inputserver | `message` | Gets idle time from the appserver (`AR_GET_IDLE_TIME` reply, `server/sapplication.cpp:567`), or from inputserver once that's the source of events. Drop the `while(1)` in its `main.cpp` |
| 8 | unixserver | No; it's what `start.sh` does after dlogin | `Server` | wants syslogserver | `message` | See below |

**unixserver** owns all Unix daemons and init.d scripting (Rick confirmed the scope on 2026-11-08). Init stays Syllable-shaped and knows nothing about shell scripts. Everything POSIX goes through unixserver, which init sees as one service. unixserver:
- does the Unix setup `start.sh` does now: the `/applications` and `/documentation` symlinks, `/etc/profile`'s environment, emptying `$TEMP`, `network-start.sh` and dhcpc;
- runs **init.d scripts** from `/etc/init.d/`, the usual SysV way: each script takes `start`, `stop`, `restart` and `status`. Order comes from LSB-style headers in the script (`# Provides:`, `# Required-Start:`, `# Required-Stop:`, `# Default-Start:`), so ported packages' own init.d scripts work mostly unchanged. A script whose header is missing runs after the ones that have one, in name order. Each call has a timeout, and the output goes to syslogserver tagged with the script name;
- treats the existing `tasks/setup/` and `tasks/start/` scripts (`/system/index`, `/resources/index`) as start-only init.d scripts, so packages that ship them keep working. The four shipped ones (cupsd, gdbserver, sb-agent, sshd) become real init.d scripts with `stop` and `status`;
- supervises the daemons those scripts start. Where a daemon can run in the foreground (`sshd -D`, `cupsd -f`, gdbserver, sb-agent), unixserver runs it as its own child and restarts it with the same policy init uses. Where it can only fork into the background, unixserver tracks its pidfile and checks it with `status`;
- reports each script and daemon to init as a sub-service (`unix/sshd`, `unix/cupsd`), so `service list`, `service restart unix/sshd` and the `INIT_SERVICE_CHANGED` notifications cover them too. init forwards those requests to unixserver's port, `l:unix_server`;
- on `M_SERVER_QUIT`, runs every script's `stop` in reverse order before it exits, so the Unix daemons shut down cleanly as part of 2.11's ordered shutdown;
- replaces the `/system/config/sshd`-style "on" and "off" files with enabled and disabled scripts (`service enable unix/sshd`), with the old files read once and migrated.

Moving `Network --detect` out of the boot path (it becomes a notification after login) also fixes item 10.

**The registrar** stays as its own server (Rick confirmed this on 2026-11-08). It's the ninth service: it `requires` appserver, and loginserver depends on it because loading the registrar's per-user database is a login step. Its changes are the same as the media server's: drop the `while(1)` in `main()`, send `M_SERVER_READY` (until then, `ready = port:l:registrar`), save every logged-in user's database on `M_SERVER_QUIT`, and use `os/System/ProcessHasQuit` in place of the appserver's forwarded `-1` message.

## 2.11 Shutdown

1. `shutdown`, the Dock and dlogin send `M_SYSTEM_SHUTDOWN` to `l:init_server` instead of `DR_CLOSE_WINDOWS` to the appserver. If `l:init_server` doesn't exist (an old init), they fall back to today's path.
2. Init asks loginserver to end the session. loginserver asks the appserver to close user apps (the `CloseWindows()` loop without its final `apm_poweroff()`/`reboot()` calls) and sends `M_SERVER_SAVE_USER_CONFIG`.
3. Init stops services in reverse dependency order: `M_SERVER_QUIT`, wait up to `stop_timeout`, then `SIGTERM`, then `SIGKILL`.
4. syslogserver stops last, so the whole shutdown is in the log.
5. `sync()`, then `apm_poweroff()` or `reboot()`. The kernel's own SIGTERM/SIGKILL sweep (`kernel.c:568-600`) is then only a safety net.

## 2.12 Kernel changes

1. Reset `g_hDisplayServerPort` to -1 when that port is deleted, so new `Application`s wait for the next appserver. Until then, init does it by calling `set_app_server_port(-1)` (Section 2.5).
2. Send the process-death `-1` message with a timeout, not `INFINITE_TIMEOUT` (`fork.c:171`), so a hung appserver can't stop processes from exiting.
3. Later: only init (or root) may call `set_app_server_port()`.
4. Later: let any process ask for a death notification for another process on a port of its choice (a general version of the appserver's `-1` message). libserver's `ClientList` would use it in place of `ProcessHasQuit` and polling.
5. Stamp each queued message with the sender's process id, uid and gid, and return them to the receiver (`get_msg_sender()`), so servers can check who is asking (Section 2.18).

None of these block the first milestone.

## 2.13 Services: the manager app

Rick asked on 2026-11-08 for a UI to manage all the servers. This is **Services**, a preferences app next to the existing ones in `system/apps/utils/Preferences/` (Network, Login, UsersAndGroups and the rest), installed as `/applications/preferences/Services`. It's an ordinary libsyllable `os::Application` and a client of init through `os::ServiceManager` (Section 2.7). It has no special access: anything it can do, `service` on the command line can do too, and that matters when the appserver is down.

### The window

```
┌ Services ───────────────────────────────────────────────────────────┐
│ [Start] [Stop] [Restart]   [Enable] [Disable]      Show: [All     ▾]│
├─────────────────────────────────────────────────────────────────────┤
│ ● syslogserver     Running   pid 14   up 2h 03m   restarts 0        │
│ ● appserver        Running   pid 16   up 2h 03m   restarts 0        │
│ ● inputserver      Running   pid 21   up 2h 02m   restarts 1        │
│ ● mediaserver      Running   pid 23   up 0h 12m   restarts 3        │
│ ○ waiterserver     Stopped   (disabled)                             │
│ ✖ clipboardserver  Failed    exit 11 (SIGSEGV), 5 restarts in 60 s  │
│ ▾ unixserver       Running   pid 19                                 │
│     ● unix/sshd    Running   pid 40                                 │
│     ● unix/cupsd   Running   pid 44                                 │
├─────────────────────────────────────────────────────────────────────┤
│ [Details] [Log] [Settings] [Dependencies]                           │
│  mediaserver: Audio and video mixing and routing                    │
│  /system/mediaserver   requires appserver   restart always          │
│  Last exit: 12:41:07, signal 11                                     │
└─────────────────────────────────────────────────────────────────────┘
```

- **The list:** an `os::ListView` (a tree for unixserver's `unix/*` sub-services), one row per service with state, pid, uptime and restart count. The state colours are green for ready, grey for stopped or disabled, amber for starting or waiting on a dependency, and red for failed. The filter shows all, running, failed, or system versus Unix services.
- **Live updates:** the app registers with `INIT_WATCH`, so rows change as init reports `INIT_SERVICE_CHANGED`, with no polling.
- **Buttons:** Start, Stop, Restart, Enable and Disable send the matching `INIT_*` requests. Stopping something others depend on (for example the appserver, which would end the session, or the registrar, which loginserver needs) asks first and lists what will stop with it. `critical` services also ask first.
- **Details tab:** the description, command line, user, boot modes, restart policy, last exit status and time, and when the next restart attempt is if one is pending.
- **Log tab:** the last lines of the service's output from init's per-service buffer (Section 2.9), or from syslogserver once that's running, with a Follow toggle and Copy.
- **Settings tab:** a form for the service file's `key = value` fields (restart policy, delays, `boot`, `enabled`, `ping` and the rest), with a raw text view for anything the form doesn't cover. Saving writes the override file in `/system/config/services/local/` (never the shipped file), sends `INIT_RELOAD`, and shows any parse error init reports on the line it came from. For `unix/*` entries the tab shows the init.d script's header and opens the script itself in AEdit.
- **Dependencies tab:** what this service requires and wants, and what depends on it, so you can see what a restart will take down.

### A Dock indicator (optional)

A small Dock plugin in `system/apps/desktop/dock_plugins/`, next to the existing ones (Meter, CpuSpeed, battery and others). It shows nothing while everything is fine and a red mark when a service is `failed`. Clicking it opens Services at that service. It uses the same `INIT_WATCH` messages, or the `os/Init/ServiceChanged` event.

### Protocol additions

The app needs three things the protocol in Section 2.7 doesn't have yet:

| Code | Fields in | Reply fields |
|---|---|---|
| `INIT_GET_LOG` | `name`, `lines` | `line`[] (oldest first) |
| `INIT_GET_SERVICE_FILE` | `name` | `path`, `override_path`, `text`, and the parsed fields |
| `INIT_GET_DEPENDENCIES` | `name` | `requires`[], `wants`[], `required_by`[] |

`INIT_GET_SERVICE` also gains `started_at`, `next_restart_at` and `boot`, and `INIT_RELOAD` returns `error_line` and `error_text` when a file doesn't parse. `INIT_GET_LOG` covers the `service log <name>` command as well.

### Access

Everything runs as root today (Section 1.3, item 12), so for now anyone can stop anything. The app and the protocol should still be built for a later check: init will refuse start, stop, enable and edit requests from non-root users, and the app will show read-only controls when init says so. Init can do that check because each request carries the sender's process.

### When the appserver is down

Services is a GUI app, so it can't help while the appserver itself is down or restarting. That's what the `service` command (Section 2.7) and the `text` and `rescue` boot targets (Section 2.4) are for. The two tools share `os::ServiceManager`, so they can't drift apart.

## 2.14 dlogin, Desktop and Dock: session components, not system servers

Proposed on 2026-11-08, in answer to Rick's question about whether these should be servers.

**Recommendation:** none of the three should be an init service. Desktop and Dock should be **session services**, supervised by loginserver with the same machinery. dlogin should be loginserver's login window.

Why they aren't system servers:
- Init services are system-wide. They start at boot, run as a fixed user, and live until shutdown. Desktop and Dock exist only while someone is logged in, must run as that user, and must stop at logout.
- dlogin today does two jobs: it's the login window, and it's the session manager (it forks `/system/programs/desktop` as the user and waits for it, `apps/utils/dlogin/commonfuncs.cpp:120-150`). The session-manager half is what loginserver takes over (Section 2.10). The window that's left is a UI, not a server.
- Desktop starts the Dock itself (`apps/desktop/desktop/desktop.cpp:265-269`) and then the user's startup programs (`:283-287`). Nothing restarts either one if it crashes.

How it would work:
1. Init supervises **loginserver**, which is a normal init service (requires appserver and registrar).
2. loginserver shows **dlogin** as its login window. dlogin is a normal app that sends the user name and password to loginserver and shows the result. If dlogin crashes, loginserver starts it again.
3. On login, loginserver starts the **session services** as the user, in order, from service files in the same `key = value` format: `/system/config/session/` for the system's, and `~/Settings/Session/` for the user's own additions and overrides. The defaults would be:
   - `desktop`: restart always;
   - `dock`: requires desktop, restart always;
   - the user's startup programs: restart never. These are the ones Desktop runs today.
4. loginserver uses libserver's supervision code (the same reaper, backoff and restart limits init uses), so there's one implementation. Desktop no longer forks the Dock.
5. On logout, loginserver stops the session services in reverse order with `M_SERVER_QUIT` and then `SIGTERM`, sends `M_SERVER_SAVE_USER_CONFIG` to the system servers, and shows dlogin again.
6. Desktop and Dock get the libserver handshake through `GuiServer` (Ready, Quit and SaveState) but keep running as apps. They lose the special cases in `CloseWindows()` (`server/server.cpp:1043-1054`), because loginserver now decides the order at logout.
7. loginserver reports the session services to init as sub-services, as unixserver does, under `session/desktop` and `session/dock`. They show up in the Services app (Section 2.13) and the `service` command, so restarting a stuck Dock is `service restart session/dock`.

The result is three tiers with the same file format, the same messages and the same supervision code: init for system servers, unixserver for Unix daemons and init.d scripts, and loginserver for each user's session.

## 2.15 User servers (assistant_server and others)

Rick asked on 2026-11-08 about user-specific servers, such as a server for the assistant. Yes: the session tier in 2.14 is where they go. A **user server** is a session service that is also a real libserver server: it has a port, a protocol and clients, and it runs as the logged-in user for as long as they're logged in.

What makes it different from a system server:

| | System server | User server |
|---|---|---|
| Supervised by | init | loginserver |
| Service file | `/system/config/services/` | `/system/config/session/` (shipped) or `~/Settings/Session/` (the user's own) |
| Runs as | its `user` field, root today | the logged-in user |
| Lives | boot to shutdown | login to logout |
| Port | `l:<name>` | `l:<name>:<uid>` |
| Config | `/system/config/<name>` | `~/Settings/<name>` |
| In the Services app | under its name | under `session/<name>` |

**Port names carry the user.** Port names are system-wide, and a second `make_port_public()` with the same name fails with `-EEXIST` (`kernel/kernel/msgport.c:590-595`). Only one user has the desktop at a time, but another user can be logged in over ssh, so two users' servers must not clash. libserver handles this: a server built with `os::Server( "assistant_server", os::Server::PER_USER )` publishes `l:assistant_server:<uid>`, and `ServerLink` looks up the caller's own uid's port by default. Neither side writes the suffix.

**Build:** `syllable_server( assistant_server ... SESSION )` installs the service file in `/system/config/session/` rather than `/system/config/services/`.

**Example: assistant_server.** The assistant (`system/apps/desktop/assistant`, see `docs/assistant-plugins.md`) already has the right split inside one process. `ProviderThread` is an `os::Looper` that owns every provider plugin and talks only through messages. The UI (the hotkey, `MainWindow` and `TopView`) is a separate thread that sends queries and gets results back. So:
- **assistant_server** is `ProviderThread` and the providers, moved into a libserver `Server` (no GUI needed) on `l:assistant_server:<uid>`. Its protocol is the query, result, launch and cache messages that `messages.h` already has. It loads the providers once at login, keeps their caches and node monitors running in the background, and writes its caches on `M_SERVER_QUIT`.
- **The Assistant app** keeps the hotkey and the popup window, and talks to the server through an `AssistantManager` built on `ServerLink`. It opens instantly because the index is already warm.
- **Other programs can ask it too.** The Dock, FileBrowser or a command-line `assistant query` tool can use the same `AssistantManager`.
- If a provider plugin crashes, it takes down assistant_server and not the window, and loginserver restarts the server.
- Its service file:

```
# /system/config/session/assistant_server
description = Search and launch providers for the Assistant
exec        = /system/servers/assistant_server
requires    = registrar
ready       = message
restart     = always
```

Other user servers that would fit the same pattern later: a per-user notification server, a per-user settings or keyring server, and per-user sync or indexing services.

## 2.16 notification_server, and where the OSD goes

Rick asked on 2026-11-08 for a notification server that follows the freedesktop.org **Desktop Notifications Specification** (version 1.3, August 2024), **without D-Bus**. The spec's *schema* is used exactly: its methods, arguments, hints, capabilities, close reasons and markup. The *transport* is Syllable messages on a port. D-Bus isn't ported (CUPS is built with `--disable-dbus`, `cmake/ports/40-cups.cmake:39`), and it isn't needed.

### Where it runs

notification_server is a **user server** (Section 2.15). It's supervised by loginserver, runs as the logged-in user, and has the port `l:notification_server:<uid>`. Notifications belong to whoever is at the screen, and their history and settings live in `~/Settings/notifications`.

System servers running as root (init, unixserver, mediaserver and the rest) also need to notify, for example "service X failed" or "disk almost full". They don't know which user is at the screen, so libserver's `NotificationManager` asks loginserver for the active session and sends to that user's server. If no one is logged in, the notification goes to syslogserver only.

It's built on `os::GuiServer`, because it draws its own popup windows.

### The spec mapped to messages

The four D-Bus methods become request codes and the three signals become messages to watchers. Field names and types are the spec's, with `os::Message` types standing in for D-Bus ones.

| Spec | Message | Fields (spec name → Message type) | Reply |
|---|---|---|---|
| `Notify` | `NOTIFY` | `app_name` String, `replaces_id` int32 (0 = new), `app_icon` String, `summary` String, `body` String, `actions` String[] (key, label, key, label, ...), `hints` Message (below), `expire_timeout` int32 | `id` int32 |
| `CloseNotification` | `NOTIFY_CLOSE` | `id` int32 | none |
| `GetCapabilities` | `NOTIFY_GET_CAPABILITIES` | none | `capability` String[] |
| `GetServerInformation` | `NOTIFY_GET_SERVER_INFO` | none | `name` "notification_server", `vendor` "Syllable", `version`, `spec_version` "1.3" |
| signal `NotificationClosed` | `NOTIFY_CLOSED` | `id`, `reason` int32: 1 expired, 2 dismissed by the user, 3 closed by `NOTIFY_CLOSE`, 4 undefined | (sent) |
| signal `ActionInvoked` | `NOTIFY_ACTION_INVOKED` | `id`, `action_key` String | (sent) |
| signal `ActivationToken` | `NOTIFY_ACTIVATION_TOKEN` | `id`, `activation_token` String | (sent) |

How the signals reach the sender: D-Bus broadcasts them. Here, `NOTIFY` carries the sender's reply port (`Messenger` already does this), and notification_server sends the three messages only to that port. A program such as a notification history Dock plugin can also register with `NOTIFY_WATCH` to get every notification's events, the way `INIT_WATCH` works.

`expire_timeout` follows the spec: -1 means the server's default, 0 means never expire, and anything higher is milliseconds.

**Hints** are a nested `os::Message` keyed by the spec's names:
- `urgency` int8 (0 low, 1 normal, 2 critical);
- `category` String;
- `desktop-entry` String. On Syllable this is the application's MIME name, such as `application/x-vnd.syllable-Webster`, and the registrar supplies its icon;
- `image-data` as a nested Message with `width`, `height`, `rowstride`, `has_alpha`, `bits_per_sample`, `channels` and `data` (raw bytes), the spec's `(iiibiiay)`;
- `image-path` String;
- `sound-file`, `sound-name` String, `suppress-sound` bool. The sound is played through mediaserver;
- `resident`, `transient`, `action-icons` bool;
- `x`, `y` int32.

It also reads the deprecated `image_data` and `image_path` names. Unknown hints are kept and ignored, as the spec requires.

**Capabilities** it reports: `actions`, `body`, `body-markup`, `body-hyperlinks`, `body-images`, `icon-static`, `persistence`, `sound`, `action-icons`, plus `x-syllable-osd` for the OSD below. Body markup is the spec's subset: `<b>`, `<i>`, `<u>`, `<a href>` and `<img src alt>`. Anything else is shown as plain text.

**Categories** are the spec's list (`device.added`, `email.arrived`, `im.received`, `network.connected`, `transfer.complete` and the others), plus Syllable ones under `x-syllable.*`, such as `x-syllable.service.failed`.

### Client side

- `os::NotificationManager`, built on `ServerLink`, so it finds the server and reconnects after a restart:

  ```cpp
  os::Notification cNote( "Webster", "Download finished", "<b>file.zip</b> is in Downloads" );
  cNote.SetCategory( "transfer.complete" );
  cNote.AddAction( "open", "Open" );
  cNote.SetTarget( pcMyHandler );   // gets NOTIFY_ACTION_INVOKED and NOTIFY_CLOSED
  uint32 nId = os::NotificationManager::Get()->Notify( cNote );
  ```

- A `notify-send` command with the same options as the freedesktop one (`-u`, `-t`, `-i`, `-c`, `-a`, `-h`, `-A`), so scripts and ported software's shell hooks work.
- Later, if ported GTK software wants `libnotify`, a small `libnotify` whose functions send these messages instead of D-Bus. The schema is the same, so the mapping is one to one.

### What the server does with them

- Shows popups in a corner stack (top-right by default; configurable), honouring `x` and `y`, `urgency` (critical ones don't expire unless the sender says so), `replaces_id`, `resident` and `transient`.
- Keeps a history of non-transient notifications in `~/Settings/notifications`, shown by a Dock plugin (a bell with a count) that also has Do Not Disturb.
- Holds popups back while the screensaver is running or a full-screen app has focus, asking waiterserver and the appserver, and shows them afterwards. Critical ones always show.
- Per-application settings (allow, mute, no popups) in a new preferences page, keyed by `desktop-entry` or `app_name`.

### The OSD: same server, separate API

**Recommendation: put the OSD in notification_server, but give it its own messages and not `NOTIFY`.**

By the OSD I mean the short centred overlay shown for volume, brightness, Caps Lock, Num Lock, keyboard layout, display mode changes and similar. The spec has no OSD; GNOME and KDE both draw theirs outside the notification service.

Why the same process:
- Both are small, always-on-top, non-focus-taking popup windows with the same theme, fonts and screen placement logic. Two processes would mean two copies of that, and the two would fight over the same screen space.
- Both have to know about the screensaver and full-screen apps.
- It's one fewer server for loginserver to start in every session.

Why a separate API:
- OSDs aren't notifications. They replace themselves in place on every key press, they disappear in about a second, they have no actions, and they must never go into the history or count toward Do Not Disturb.
- Old notify-osd-style hacks (sending volume through `Notify` with a private hint) are what that separation avoids. A clean API also means the OSD can move into its own server later without changing any caller.

The OSD messages:

| Message | Fields | Meaning |
|---|---|---|
| `OSD_SHOW` | `kind` String (`volume`, `brightness`, `capslock`, `numlock`, `keymap`, `display`, or `x-...`), `icon` String, `label` String, optional `level` float (0 to 1, draws a bar), optional `muted` bool | Shows, or updates in place if one of the same `kind` is up |
| `OSD_HIDE` | `kind` | Hides it early |

`os::OSD::Show( "volume", 0.65f )` wraps it in libsyllable.

Who sends them: mediaserver for volume and mute, inputserver for Caps Lock, Num Lock and layout changes (once input moves there, Section 2.10), and the appserver for display mode changes. The keyboard work in `keyboard/numlock-super-key-2026-11-03.md` is where the Caps Lock and Num Lock ones would hook in.

The OSD is per user like notifications, and it's drawn by whichever session has the screen. There's one more case: the login screen has no session, so notification_server isn't running. loginserver runs a session-less instance of notification_server for the login screen, OSD only, so Caps Lock still shows while you type your password.

## 2.17 translation_server: images now, other formats later

Rick asked on 2026-11-08 to move the translators into their own server: all image formats first, and other format types later.

### Today

- The translators are shared libraries in `/system/extensions/translators/`: bmp, gif, ico, jpeg, pcx, png, svg, tga, tiff, xbm and xpm (`system/sys/translators/`).
- **Every process that decodes an image loads all of them.** `TranslatorFactory::LoadAll()` calls `load_library()` on every file in that directory (`libsyllable/translation/translator.cpp:65-125`), and `BitmapImage::Load()` uses the factory (`libsyllable/gui/image.cpp`). So every app maps libpng, libjpeg, libtiff and, through the SVG translator, librsvg and cairo. A crash in a decoder takes the app down with it.
- The plugin API is small: a `TranslatorNode` reports one `source_type → dest_type` pair with a `quality` (`TranslatorInfo`, `include/syllable/translation/translator.h:109-114`), answers `Identify()` by sniffing data, and makes a streaming `Translator` (`AddData()`, `Read()`). Images come out as `image/x-atheos_trans`: a `BitmapHeader` followed by `BitmapFrameHeader`s and pixels.
- Two branches already touch this: `origin/feat/move-libtranslation` (586b33e3, starts splitting libtranslation out of libsyllable) and `origin/translator-changes` (4acd8509, splits `translator.h` into several headers and updates every translator). Both should be folded into this work rather than redone.

### The server

- **A system server**, `l:translation_server`, built on libserver's `os::Server`. It needs no appserver, so it can start right after syslogserver, and nothing waits on it, including the appserver.
- It's the **only process that loads translator plugins.** It loads them once at boot, rescans `/system/extensions/translators/` (and a package directory under `/resources/index/`) with a node monitor, and keeps the format table.
- It runs as root today, like everything else. Because it decodes untrusted files, it should be the first server to get its own unprivileged user once users matter (Section 1.3, item 12). Its service file should set `user` from the start.
- `restart = always`. A crashing decoder now kills the server, init restarts it, and the app gets an error for that one image.

### The protocol

Pixels don't go through ports. A request carries the source, and the result comes back in a shared **area** the client maps, as the media server and the appserver already do for large data (appserver review, Sections 3.1 and 5.1).

| Message | Fields in | Reply |
|---|---|---|
| `TRANS_IDENTIFY` | `path` String, or `data` (the first few KB), optional `hint_type` (from the extension) | `type` (MIME), `confidence` float |
| `TRANS_LIST_FORMATS` | optional `family` (`image`, later `text`, `document` and others) | per format: `type`, `description`, `extensions`[], `can_read`, `can_write`, `quality` |
| `TRANS_CONVERT` | the source as `path` or `area` + `size`, `src_type` (optional: it identifies if missing), `dst_type`, `config` Message (quality, frame, size limits and the like) | `area` (the result), `size`, `dst_type`, and for images the `BitmapHeader` fields and frame count |
| `TRANS_OPEN_STREAM` / `TRANS_STREAM_DATA` / `TRANS_STREAM_CLOSE` | for data that arrives in pieces (a web download): a stream id and chunks | progressive results in an area, so Webster can draw a partly loaded image |
| `TRANS_RELEASE` | `area` | none (the server frees its side) |

`path` is the fast case: the server reads the file itself, so nothing is copied through the client. Decodes run on a small pool of worker threads, each request has a timeout, and a request that times out gets an error. A hung decoder can only be stopped by restarting the server, which init does. Isolating each plugin in its own worker process is a later option if that turns out to matter.

### The client side: no app changes

`TranslatorFactory`, `Translator` and `BitmapImage::Load()` keep their public APIs, so applications don't change. Inside, the factory becomes a **client** (a `TranslationManager` built on libserver's `ServerLink`) that sends `TRANS_CONVERT` or a stream and maps the area that comes back. If the server isn't running (early boot, text mode, or a server restart in progress), the factory falls back to loading the plugins in-process the way it does now, so nothing breaks. A setting can turn the fallback off once the server is trusted.

The appserver's own images (decorators, cursors, icons) can use the same path, because translation_server never depends on the appserver.

### Plugin API, version 2

To list and identify formats without creating a translator, `TranslatorInfo` grows the metadata the server needs: MIME `type`, `description`, `extensions`, magic-byte patterns for sniffing, and whether the plugin reads, writes or both. Version 1 plugins keep loading (the server fills the gaps by calling `Identify()`). The `origin/translator-changes` header split is the natural place to add this.

### "Eventually format types": how this fits with the registrar

The two servers split the work this way:
- **translation_server knows what data *is* and how to convert it.** It sniffs content, lists the formats it can read and write, and converts between them, including chained conversions (for example `image/x-foo → image/x-atheos_trans → image/png`) when no single plugin does it.
- **The registrar knows what each user wants done with a type** (Section 2.10): descriptions, icons, the handler app and the default handler, per user.

When the registrar meets a file it can't type by extension, it asks translation_server to identify the content. When translation_server gets a new plugin, it can register the new MIME types with the registrar, so FileTypes and the Open With menu learn about them automatically.

Format families after images, once the protocol has proven itself with images: text encodings and line endings, rich text and documents (RTF, HTML or Markdown to text and back), and archive listings. **Audio and video codecs stay with mediaserver**, because they're real-time streams and the media server already owns their plugins. They can show up in `TRANS_LIST_FORMATS` for information.

Later, translation_server is also the natural owner of a thumbnail cache, for FileBrowser and the Assistant. It could follow the freedesktop Thumbnail Managing Standard's layout and naming (`~/.cache/thumbnails/`), in the same "their schema, our transport" way as the notification server.

## 2.18 Who may stop a service

There are two ways to stop a service: asking init (`INIT_STOP_SERVICE` from the `service` command, the Services app, or any program), and sending a signal directly (`kill`). The design handles both.

### 1. A control policy in each service file

A new key, `control`, says who may stop, restart or disable the service through init:

| `control` | Who may stop, restart or disable it through init | Used for |
|---|---|---|
| `system` | **No one.** Init only stops it at shutdown, or when a dependency it `requires` stops. Requests are refused with `EPERM`, even from root | init itself, syslogserver, appserver, translation_server, registrar, loginserver |
| `admin` | root (and later an `admin` group) | inputserver, mediaserver, clipboardserver, waiterserver, notification_server, unixserver and its daemons |
| `user` | root, or the user the service runs as | user servers and session services (assistant_server, Desktop, Dock) |

The default is `admin`. `restart` requests are allowed one level more freely than `stop`: restarting the appserver is an emergency tool, so root may ask for it even though it's `system`, but nobody may simply turn it off.

- **Editing the policy needs root** and goes through the override file in `/system/config/services/local/`. A `system` service can't be disabled with `INIT_DISABLE_SERVICE`. That needs editing the file and `INIT_RELOAD`, so it can't happen by accident.
- **The rescue target ignores the policy.** Booting with `bootmode=rescue` gives a shell where `service stop --force` works on everything, for the case where a broken service has to be turned off.
- **Refused requests are logged** at `notice`, with the sender, so attempts are visible.

### 2. Confirmation for the dangerous but allowed cases

The Services app and the `service` command ask before:
- stopping anything others `require`, listing what will stop with it. On the command line that's `service stop mediaserver --with-dependents`; without the flag the request is refused with the list;
- restarting the appserver or loginserver (which ends the session);
- stopping a service marked `critical`.

### 3. Signals: init brings it back

`kill -9` doesn't go through init. The kernel's signal check is the POSIX one: the sender's real or effective uid must match the target's real or saved uid, or the sender must be root (`kernel/kernel/psignals.c:820-833`). Today that stops nothing, because everything runs as root.

So the answer for signals is supervision, not prevention: a `system` or `admin` service that dies from a signal it didn't ask for is restarted by init straight away, without the restart backoff, and logged at `warning` with the signal number. A user can't kill other users' services once services run as their own users (Section 1.3, item 12): `user` services run as the user, system servers as root or their own service users, and the kernel check already blocks the rest.

### 4. The missing piece: knowing who sent a request

For init to check the policy, it has to know who's asking. **Today it can't:** a kernel message is just a code, a size and the data (`kernel/kernel/msgport.c:259-269`). Nothing records the sending process. A uid in the message body could be faked by anyone.

So this needs one more kernel change (Section 2.12, item 5): when a message is queued, the kernel stamps it with the sender's process id, uid and gid, and a new `get_msg_sender()` (or extra out-parameters on `get_msg_x()`) returns them to the receiver. libsyllable's `Message` gets `GetSenderProcess()` and `GetSenderUID()`. Every server benefits, not just init: syslogserver tags records reliably, notification_server knows which app sent a notification, and the registrar knows which user is asking.

Until that change is in, init applies `control = system` (refuse) for everyone and treats every other request as if it came from root, which matches today's root-by-default system. The policy keys and the checks are in place from the start, so turning them on is one kernel change and no service changes.

---

# Part 3: Plan

Each step boots in QEMU with `sb run` and passes the existing boot test, which reads the serial log. Init prints `PYRO-INIT:` markers for the test to check: services started, ready, restarted, stopped.

1. **M0, boot fixes (small, can go in now).** `init.c` runs `early-start.sh` only if it exists. Ship an empty one in `system/image/system/`. `start.sh` reads `$1`.
2. **M1, init_server with today's boot.** Looper, service files read with libserver's `ServerConfig`, start in dependency order, readiness by port, no restarts yet. Service files for appserver, mediaserver, registrar and dlogin. The rest of `start.sh` (network, inetd, tasks) runs as one `unix-legacy` service that is `start.sh` with those four lines removed. Test: the desktop comes up as it does today, and the `PYRO-INIT` markers appear in order.
3. **M2, monitoring and restart.** The reaper thread, restart policy and backoff, dependent restarts, `set_app_server_port(-1)`. Test: kill the media server, then the appserver, from `sb execute` and check that both come back and the login screen returns.
4. **M3, protocol, tools and libserver.** `l:init_server` requests, `os::ServiceManager`, the `service` command, `os/Init/ServiceChanged`, and libserver's `Server`, `GuiServer` and `SERVER_MAIN` with the example server (Section 2.8).
5. **M4, shutdown through init.** Change `shutdown`, the Dock and dlogin. Test: `service`-driven reboot with the media server saving its settings.
6. **M5, syslogserver and log pipes.** Replace `dbterm 8`, add the libc `syslog()` change, the `/var/log/services/` files and rotation, and the `service log` and `log` commands.
   **M5b, control policy.** The `control` key and the refusals for `system` services, the confirmations in `service` and the app, and the immediate restart after an unexpected signal. Then the kernel's sender stamp (Section 2.12, item 5), which turns on the per-user checks.
7. **M6 and later, the new servers one at a time.** unixserver (replacing `unix-legacy`, with init.d support and the four shipped tasks turned into init.d scripts), then inputserver (once `feat/input-server` builds), loginserver (split from dlogin, taking over Desktop and Dock as session services, Section 2.14), clipboardserver, then waiterserver. Each is built on libserver (`SERVER_MAIN`, a service file), which removes its `while(1)`. The media server and registrar move onto libserver here too.
8. **M7, the Services app.** It can start once M3 is in, because it only needs the protocol and `os::ServiceManager`, so it can be built alongside M4 to M6. First the list, live updates and the buttons, then the Log, Details and Dependencies tabs, then the Settings editor and the Dock indicator.
9. **M8, user servers.** `PER_USER` ports in libserver, `SESSION` in the CMake helper, and session service files from `~/Settings/Session/`, after loginserver exists. assistant_server is the first: move `ProviderThread` and the providers out of the Assistant app.
10. **M9, notification_server.** A user server on libserver: `NOTIFY` and the other spec messages, popups, `notify-send`, `os::NotificationManager`, then the OSD messages, history, the Dock plugin and Do Not Disturb. Test: `notify-send` from `sb execute` in QEMU, checking the `NOTIFY_CLOSED` reasons and the action messages.
11. **M10, translation_server.** Fold in `feat/move-libtranslation` and `translator-changes` first. Then the server with `TRANS_IDENTIFY`, `TRANS_LIST_FORMATS` and `TRANS_CONVERT` by path, the `TranslatorFactory` client with its in-process fallback, then streams for Webster, plugin API version 2 and the registrar link. Test: images still load in Webster, AView and FileBrowser icons with the server running, with it stopped, and after `kill -9` while decoding. It doesn't depend on loginserver, so it can go any time after M3.

## Decisions

- **Registrar:** stays as its own server (2026-11-08).
- **unixserver:** owns all Unix daemons and init.d scripting (2026-11-08).
- **Service files:** plain `key = value` text for now (2026-11-08).
- **libserver:** one base library all servers are built on (2026-11-08, Section 2.8).
- **Services app:** a UI to manage all the servers (2026-11-08, Section 2.13).
- **User servers:** per-user servers such as assistant_server, supervised by loginserver (2026-11-08, Section 2.15).
- **notification_server:** follows the freedesktop Desktop Notifications Specification's schema over Syllable messages, no D-Bus (2026-11-08, Section 2.16).
- **OSD:** proposed as part of notification_server with its own messages (Section 2.16). Waiting for Rick's call.
- **translation_server:** all translators move to their own server, images first, other format types later (2026-11-08, Section 2.17).
- **Stop protection:** a `control` policy per service, enforced by init, with the kernel's sender stamp to identify callers (Section 2.18).
- **dlogin, Desktop and Dock:** proposed as session components, not system servers (Section 2.14). Waiting for Rick's call.
