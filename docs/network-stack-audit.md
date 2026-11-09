# Syllable (Pyro) network stack audit — 2026-11-08

This is a read-only audit of the network code in `/Users/rcaudill/Code/Rick-PyroOS` (the `Syllable` repo at `963b292e`, plus `Syllable-Build-System/out/libraries/libc.so.2`). I didn't build, run, change or commit anything, and I didn't touch the running `sb` build. It covers the kernel stack and the glibc 2.7 network functions. It's a companion to `docs/posix-audit-2026-10-30.md`, which already covers AF_UNIX, IPv6, poll and select at the POSIX level. This doc goes deeper on the network stack itself.

## Summary

The kernel stack is the AtheOS stack written by Kurt Skauen around 2000, with its TCP derived from Comer's Xinu. It's about 13,200 lines in `kernel/net/`. It has IPv4, TCP, UDP, raw sockets, ICMP echo, ARP, a route table, a loopback device, IP fragmentation and reassembly, and 16 Ethernet drivers plus TUN. The basic data path works: DHCP, DNS, HTTP and ssh all run over it. What it doesn't have is the hardening and the edge-case behaviour that modern software relies on.

The problems, most important first:

1. **No user-pointer checks in the socket syscalls.** `recvmsg`, `sendmsg`, `connect`, `bind`, `accept`, `getsockname`, `getpeername` and the route ioctls read and write user memory directly. They never call `verify_mem_area` or `memcpy_*_user`. Any local program can make the kernel write into kernel memory, which is a privilege escalation, and a bad pointer panics the kernel.
2. **Malformed packets from the network can corrupt kernel memory.** `ip_in()` doesn't validate the IP header. `udp_cksum()` and `udp_recvmsg()` trust the length field in the UDP header, and `ip_chksum()` writes a pad byte past odd-length buffers. One crafted UDP packet can read or write past the packet buffer.
3. **`setsockopt(SO_RCVBUF/SO_SNDBUF)` overflows the kernel heap** whenever the new size is smaller than the old one. It also scrambles the ring buffer when the size grows. Ordinary programs set these options.
4. **No permission checks anywhere in `net/`.** Any user can open raw sockets (and spoof packets), change interface addresses, MTU and flags, add and delete routes, and bind ports below 1024.
5. **TCP connect, bind and getpeername give wrong answers.** A blocking `connect()` to a closed port **returns 0**, and the error only shows up on the first read or write. `bind()` never returns EADDRINUSE: two servers can bind the same port without an error. `connect()` ignores an earlier `bind()`. `getpeername()` returns the port in the wrong byte order.
6. **`socket()` doesn't check the address family.** AF_UNIX and AF_INET6 sockets are quietly created as IPv4 sockets. That misleads glibc's `getaddrinfo` (it's the likely cause of the sshd IPv6 assertion) and every configure script.
7. **Socket options and flags that are missing or ignored:** SO_RCVTIMEO, SO_SNDTIMEO and SO_LINGER (EINVAL); MSG_DONTWAIT, MSG_NOSIGNAL, MSG_WAITALL and MSG_TRUNC (ignored); FIONBIO on sockets (ENOSYS); multicast (not there at all). UDP has no getpeername, getsockopt or shutdown.
8. **ICMP is echo only.** Incoming "destination unreachable" is dropped, so a connect to a dead host blocks for up to **9 minutes**. The stack never sends "port unreachable". Every outgoing packet has DF set, but "fragmentation needed" is ignored, which black-holes traffic on paths with a smaller MTU.
9. **glibc:** the `if_nametoindex`, `if_indextoname` and `if_nameindex` family and `sendfile` are ENOSYS stubs. `recvfrom(fd, buf, n, 0, NULL, NULL)` crashes. `getsockopt` never writes `*optlen` back. The image ships no `/etc/services`, `/etc/protocols` or `/etc/nsswitch.conf`.
10. **Missing features** (big jobs): AF_UNIX with SCM_RIGHTS, IPv6, TCP window scaling, SACK and timestamps, fast retransmit, multicast/IGMP, and drivers for e1000 and virtio-net.

Fixing items 1–6 is mostly small, local changes in `socket.c`, `ip.c`, `udp.c` and `tcp.c`. Those fixes buy the most safety and compatibility for the effort. The roadmap at the end gives the order.

## Method and paths

- **Kernel:** `Syllable/system/sys/kernel/kernel/net/` (shortened to **`net/`** below): `socket.c` (the BSD socket layer and the socket "filesystem"), `ip.c` (IP, ICMP, reassembly), `tcp.c`, `tcp_input.c`, `tcp_output.c`, `tcpdefs.h`, `udp.c`, `raw.c`, `arp.c`, `route.c`, `interface.c`, `eth_interface.c`, `core.c` (loopback and dispatch) and `packets.c`. Headers: `Syllable/system/sys/include/syllable/net/`. Drivers: `Syllable/system/sys/kernel/drivers/net/`. Syscall numbers 155–168 in `syllable/syscall.h`.
- **The libc that ships:** glibc 2.7 with the `syllable` add-on, `Syllable/system/apps/utils/Builder/packages/Syllable/glibc-2.7/patches/syllable/sysdeps/unix/sysv/syllable/` (shortened to **`GP/`**), built by `Syllable-Build-System/toolchain/scripts/06-glibc.sh`. I read the exported symbols of the built `out/libraries/libc.so.2` with `nm -D`. I took the ENOSYS stub list from the "is not implemented and will always fail" link warnings in the same file. (The `out/sysroot` tree that the POSIX audit used isn't there right now, probably because the `sb` build is running.)
- **Userland, for context only:** `apps/utils/dhcpc`, `apps/utils/nettools` (ifconfig, route, ping), `apps/utils/Preferences/Network*` and `system/image/etc/`.

Line numbers are for the current `main`.

## Kernel findings

### A. Socket layer (`net/socket.c`)

| # | Severity | Finding | Where |
|---|---|---|---|
| A1 | **Critical (security)** | **User pointers are never validated or copied.** `do_connect`/`do_bind` dereference the user's `sockaddr` (`psInAddr->sin_family`) before anything else, with no length check. `sys_accept` has `// FIXME: Validate the pointers!!!`. `sys_sendmsg`/`sys_recvmsg` hand the user's `msghdr` straight to the protocol, which then `memcpy`s to and from `msg_iov[i].iov_base` (`tcp.c:658-670, 800-812`, `udp.c:650, 669`; `raw.c` has no user copies at all). `getsockname`/`getpeername` write straight into the user's `sockaddr`. The kernel shares the address space with user code, so a user can pass a *kernel* address as an iov or sockaddr and get arbitrary kernel reads and writes. A NULL or wild pointer is a kernel page fault. | `socket.c:752-764, 831-843, 1001-1004, 1019-1132`; `interface.c` is the only file that does this right (`memcpy_from_user`, `:246, :979, :1080`) |
| A2 | **High** | **`create_socket()` never checks `nFamily`.** AF_UNIX, AF_INET6, AF_PACKET or any other value gets an IPv4 TCP, UDP or raw socket, chosen only by `nType` and `nProtocol`. `bind`/`connect` later fail with EAFNOSUPPORT, which is too late for callers that probe with `socket()`. It should return EAFNOSUPPORT up front. | `socket.c:67-136` |
| A3 | High | **Route ioctls work on the raw user pointer and have no permission check.** `SIOCADDRT`/`SIOCDELRT` cast `pBuf` to `struct rtentry *` and read it directly (`route.c:985-1006`). Any user can change the routing table. | `socket.c:1461-1466` |
| A4 | Medium | **SO_LINGER, SO_RCVTIMEO, SO_SNDTIMEO, SO_RCVLOWAT and SO_SNDLOWAT return EINVAL** for both set and get. Receive and send timeouts are widely used (DNS clients, curl fallbacks, Python `settimeout` on some paths, Java, Go), and SO_LINGER is used by many servers. | `socket.c:379-385, 563-569` |
| A5 | Medium | **`getsockopt` takes `int nOptLen` and never writes the length back.** glibc passes a `socklen_t *`, which the kernel reads as an int. Every option writes 4 bytes whatever the caller's buffer size. | `socket.c:468, 598` |
| A6 | Medium | **FIONBIO isn't handled on sockets.** It falls through to `tcp_ioctl`/`udp_ioctl`, which return ENOSYS. FIFOs and ptys do handle it (`vfs/fifo.c:497`, `vfs/pty.c:1837`). Code that uses `ioctl(FIONBIO)` instead of `fcntl(O_NONBLOCK)` breaks. | `socket.c:1454-1489`, `tcp.c:1240-1262` |
| A7 | Low | `do_listen` sets `sk_bListening = true` even when the protocol's listen fails, so a later `SO_ACCEPTCONN` lies. | `socket.c:932-936` |
| A8 | Low | `IP_TOS` returns success and is ignored. The TTL is hard-wired to 128 in `ip_send_via`, so `IP_TTL` does nothing either. | `socket.c:408`, `ip.c:330` |
| A9 | Low | `fstat()` on a socket reports mode `0777` and owner root. That's harmless, but sshd and similar tools check sockets this way. | `socket.c:1437-1452` |

### B. IP and ICMP (`net/ip.c`, `net/core.c`)

| # | Severity | Finding | Where |
|---|---|---|---|
| B1 | **Critical (remote)** | **`ip_in()` validates nothing but the header checksum.** It doesn't check the version, that `iph_nHdrSize >= 5`, that the header length is no more than the total length, or that the total length fits in the received buffer. Everything downstream (UDP, TCP, ICMP, reassembly) trusts `iph_nPacketSize`. It also accepts packets for *any* destination address, not just ours or broadcast. | `ip.c:715-735` |
| B2 | High | **`ip_chksum()` writes a zero pad byte at `pBuffer[nLength]` for odd lengths.** On receive, `nLength` comes from the packet, so this is a one-byte write past the end of the buffer. `udp_cksum()` does the same thing (see C1). | `ip.c:116-120` |
| B3 | High | **ICMP only handles ECHO.** Destination-unreachable (port, host, net, frag-needed), time-exceeded, redirect and source-quench are all logged as "Unknown ICMP message" and dropped. So TCP and UDP never get ECONNREFUSED, EHOSTUNREACH or ENETUNREACH from the network. A connect to a dead host waits for the SYN retransmit limit, `TCP_RXMIT_TIMEOUT` = **540 s** (`tcpdefs.h:195`). | `ip.c:222-256` |
| B4 | High | **Every non-fragmented packet is sent with DF set, but frag-needed is ignored.** That's path-MTU discovery without the half that handles the reply, so traffic over PPPoE, VPNs or tunnels black-holes. Either clear DF or handle ICMP type 3 code 4. | `ip.c:402` |
| B5 | Medium | **An incoming ECHOREPLY is leaked.** The `ICMP_ECHOREPLY` case `break`s without `free_pkt_buffer()`, so every reply to `ping` leaks one packet buffer. | `ip.c:241-248` |
| B6 | Medium | **No ICMP errors are generated** apart from reassembly time-exceeded, which is sent without the original header and 8 data bytes that RFC 792 requires. UDP to a closed port is dropped silently (`udp.c:249-256`), so `traceroute` and UDP clients can't detect a closed port. | `ip.c:139-218, 698` |
| B7 | Medium | **Reassembly has no limits.** There's no cap on the number of queues or bytes per queue, the key is only (id, src) and not (id, src, dst, proto), and overlapping fragments aren't handled. A fragment flood can exhaust kernel memory. | `ip.c:483-640` |
| B8 | Medium | **`ip_in()` assumes a 14-byte Ethernet header in front of every packet** (`/* FIXME: Should this assume ... */`). Loopback and TUN have to fake one. That blocks PPP, raw-IP and other link types. | `ip.c:717`, `core.c:161-189` |
| B9 | Low | The IP ID counter is a non-atomic `static int16` (`/* FIXME: Use atomic_add()? */`). Two CPUs can send the same ID, which corrupts reassembly at the far end. | `ip.c:327-328` |
| B10 | Low | There's one IPv4 address per interface (no aliases, no secondary addresses). The loopback interface is called `loopback` rather than `lo`, which some software hard-codes. | `core.c:225`, `interface.c` |
| B11 | Low | There's no forwarding (fine for a desktop) and no IP options support. TCP's option stripping assumes a 20-byte IP header (D6). | |

### C. UDP (`net/udp.c`)

| # | Severity | Finding | Where |
|---|---|---|---|
| C1 | **Critical (remote)** | **The UDP length field comes off the wire and is trusted.** `udp_cksum()` uses `udp_nSize` as the loop length and writes a pad byte at `[len]`. `udp_recvmsg()` copies `udp_nSize - 8` bytes to the user. A packet with `udp_nSize = 65535` reads about 64 KB of kernel heap into user space (an information leak) and writes one byte out of bounds. Packets with a zero checksum skip the checksum but still hit the copy. | `udp.c:61-97, 644-655` |
| C2 | Medium | **Each socket queues at most 16 datagrams** and SO_RCVBUF is ignored. Bursty receivers (streaming, games, DNS servers, QUIC) drop packets. | `udp.c:206-211, 958-1013` |
| C3 | Medium | **`FIONREAD` returns the number of queued datagrams, not the size of the next one in bytes.** Code that sizes its buffer from FIONREAD (many DNS and RTP libraries) gets 1, 2 or 3 and truncates. | `udp.c:1040-1050` |
| C4 | Medium | **No MSG_PEEK, MSG_TRUNC or MSG_DONTWAIT.** An oversized datagram is cut down silently with no MSG_TRUNC flag, and `msg_flags` is never set. | `udp.c:589-676` |
| C5 | Medium | **No getpeername, getsockopt or shutdown** (they're NULL in `g_sUDPOperations`), so they return EINVAL. `getsockopt(SO_ERROR)` on UDP fails, and libuv, Python asyncio and Node all call it. | `udp.c:1059-1076` |
| C6 | Medium | **No multicast.** `IP_ADD_MEMBERSHIP` and the other multicast options fall through to `-ENOSYS`. There's no IGMP, and the kernel's `struct ip_mreq` is `#if 0`'d. mDNS/Bonjour, SSDP/UPnP and some games can't work. glibc's headers *do* advertise multicast. | `udp.c:958-1013`, `include/syllable/net/in.h:36-58` |
| C7 | Low | `bind()` doesn't check that the local address belongs to one of our interfaces (there's a FIXME). SO_REUSEADDR and SO_BROADCAST return 0 and do nothing. Sending to a broadcast address is allowed without SO_BROADCAST. | `udp.c:475-480, 962-972` |
| C8 | Low | `sendto()` with an address on a connected socket returns EISCONN. Linux allows it, and some code depends on that. | `udp.c:723-727` |

### D. TCP (`net/tcp.c`, `tcp_input.c`, `tcp_output.c`)

| # | Severity | Finding | Where |
|---|---|---|---|
| D1 | **Critical (local)** | **`SO_SNDBUF`/`SO_RCVBUF` reallocate the buffer and `memcpy` the *old* size into the *new* one.** Shrinking the buffer overflows the kernel heap. Any size is accepted, including 0, negative and huge values. The ring indices (`tcb_nRcvBufStart`, the send pointers) aren't remapped, so even growing the buffer corrupts data that's waiting in it. | `tcp.c:1332-1414` |
| D2 | **High** | **A blocking `connect()` returns 0 when the connection fails.** It returns the result of `lock_semaphore(tcb_ocsem)`, which is 0, not `tcb_nError`. A refused or reset connection looks like a success, and the error (ECONNREFUSED) only appears on the first `read`/`write`. EINTR is also turned into success (`/* Not fatal */`). | `tcp.c:376-384`, `tcp_input.c:2316-2321` |
| D3 | **High** | **`bind()` checks nothing.** There's no EADDRINUSE, no lock, no check that the address is local, and no root check for ports below 1024. Two listeners on one port both "succeed", and the hash lookup picks one of them. SO_REUSEADDR has no meaning. | `tcp.c:255-278` |
| D4 | High | **Ephemeral ports come from one `g_nPrevPort++` counter starting at 1024.** It's shared by bind(0) and connect, with no wraparound and no collision check (past 65535 it truncates into the low ports). Outgoing connections can land on ports that servers use (3000, 5432, 8080…). **`connect()` also overwrites a port and address set by an earlier `bind()`** (`tcb_lport = g_nPrevPort++`). | `tcp.c:45, 264, 339, 350` |
| D5 | High | **`getpeername()` returns `sin_port` in host byte order.** `tcb_rport` is stored in host order and copied without `htons` (accept and getsockname do it right). It also never sets `*pnNameLen` or clears `sin_zero`, and getsockname has the same two problems. sshd, logging and access control all see the wrong peer port. | `tcp.c:387-406, 415-437` |
| D6 | Medium | **Option parsing:** `pOptEnd = pOption + TCP_HLEN` runs past the end of the options (it should be `TCP_HLEN - 20`), so parsing can read into the payload. Only MSS is understood: **no window scaling, SACK or timestamps**. The option-stripping `memmove` assumes a 20-byte IP header. | `tcp_input.c:1260-1300` |
| D7 | Medium | **Throughput is capped.** The default buffers are 32 KB (`TCPSBS`/`TCPRBS`), and without window scaling the window can't go past 64 KB. There's no fast retransmit or fast recovery (no duplicate-ACK counting): every loss waits for the RTO, and cwnd collapses to one MSS. Expect a few MB/s at best on a real network. | `tcp_input.c:66-67`, `tcp_output.c:455-470` |
| D8 | Medium | **The listen backlog behaves wrongly.** `listen(fd, 0)` refuses every connection (`nNumClients >= 0`). A full backlog sends a RST instead of dropping the SYN, so clients get ECONNREFUSED under load instead of retrying. Under memory pressure it also sends a RST. The backlog isn't capped at SOMAXCONN. | `tcp_input.c:1980-1993`, `tcp.c:474` |
| D9 | Medium | **Send and receive flags:** MSG_OOB prints a warning and is ignored. MSG_DONTWAIT, MSG_WAITALL and MSG_NOSIGNAL are ignored, so SIGPIPE is *always* raised on EPIPE (`tcp.c:921, 1053`), which breaks code that relies on MSG_NOSIGNAL (curl, libuv, many servers). Only MSG_PEEK works on receive. | `tcp.c:716-730, 881-890` |
| D10 | Medium | **The ISN is predictable:** boot time plus `rdtsc % 2000`, incremented globally. That allows blind spoofing of TCP connections. RFC 6528 recommends a keyed hash. | `tcp_input.c:777-787` |
| D11 | Low | `TCP_NODELAY` and `TCP_MAXSEG` return 0 and do nothing. (There's no Nagle by default, since `TCBF_BUFFER` is off, so NODELAY is effectively already on.) TCP_KEEPIDLE, TCP_KEEPINTVL and TCP_KEEPCNT don't exist, and keepalive timing is compiled in (2 h / 75 s / 10 probes). | `tcp.c:1424-1440`, `tcpdefs.h:204-210` |
| D12 | Low | `kmalloc` of the send and receive buffers isn't checked (`// FIXME: Check for error`). Out of memory means a NULL dereference later. | `tcp_input.c:805, 809` |
| D13 | Low | TIME_WAIT is 20 s rather than 2×MSL (deliberate, see the comment). There are no SYN cookies. `select()` exceptfds (OOB) are never signalled. | `tcpdefs.h:196-197`, `tcp.c:1172-1174` |

### E. Raw sockets, ARP, routing, interfaces, drivers

| # | Severity | Finding | Where |
|---|---|---|---|
| E1 | **High (security)** | **There's no permission check anywhere in the net code.** Any user can open SOCK_RAW (and send forged IP packets), run `SIOCSIFADDR`/`SIOCSIFNETMASK`/`SIOCSIFMTU`/`SIOCSIFFLAGS` and `SIOCADDRT`/`SIOCDELRT`, and bind to ports below 1024. (`grep -E 'EPERM\|uid'` over `net/` finds nothing.) | `raw.c`, `interface.c:1193-1199`, `socket.c:1461-1466` |
| E2 | Medium | Raw sockets: SO_DONTROUTE and IP_HDRINCL routing are FIXMEs, and packets waiting on a closed raw socket are leaked ("kernel memory space just gets littered with packets"). | `raw.c:110-128, 483, 546-553, 666` |
| E3 | Medium | **The kernel has no interface index ioctls.** `SIOCGIFINDEX` and `SIOCGIFNAME` are defined but not handled in `netif_ioctl()`; they go to the driver and get ENOSYS. glibc's `if_nametoindex` family would need them even if it weren't a stub. | `interface.c:1153-1237`, `sockios.h:52-53` |
| E4 | Low | ARP adds an entry for the sender of *every* request it sees, with no check, so the cache is easy to poison. There's no gratuitous ARP on address change and no duplicate-address detection. | `arp.c:395-411` |
| E5 | Low | The Ethernet RX queue (`ei_sInputQueue`) has no length limit that I could find, so a flood can queue packets until memory runs out. | `eth_interface.c:128-140`, `packets.c:191-216` |
| E6 | Info | **Drivers:** 3c509, 3c59x, dl2k, dp83815, eepro100, ne2000, nvidia, pcnet32, rtl8139, rtl8169, sis, skge, tg3, tulip, via, plus tun. **There's no e1000/e1000e (the default NIC in QEMU, VirtualBox and VMware) and no virtio-net.** `sb run` uses rtl8139 (`syllable-vm/run.sh:34`), which works but is slow and old. | `drivers/net/CMakeLists.txt` |

## glibc findings (glibc 2.7 + Syllable add-on)

**What's there:** `socket`, `bind`, `connect`, `listen`, `accept`, `send*`/`recv*` (the add-on's `send.c`, `recv.c`, `sendto.c` and `recvfrom.c`, all built on sendmsg/recvmsg), `get/setsockopt`, `getpeername`/`getsockname`, `shutdown`, `socketpair` (emulated), `getaddrinfo`/`getnameinfo`/`gai_strerror`, `gethostbyname{,2,_r}`, `getservbyname`, `getprotobyname`, `inet_*`, `ether_*`, `rcmd`, `herror`, `getifaddrs`/`freeifaddrs`, `sockatmark`, `__res_init`, plus `libresolv.so.2.1`, `libnss_dns`, `libnss_files` and the other NSS modules. The resolver reads `/etc/resolv.conf`; NSS reads `/etc/nsswitch.conf`, `/etc/host.conf` and `/etc/gai.conf`.

| # | Severity | Finding | Where |
|---|---|---|---|
| G1 | High | **`recvfrom()` dereferences `*len` without checking it**, so `recvfrom(fd, buf, n, 0, NULL, NULL)`, which is legal and common, segfaults. | `GP/recvfrom.c` (`msg.msg_namelen = *len;`) |
| G2 | High | **The headers advertise things the kernel doesn't do:** `AF_UNIX`/`AF_LOCAL`, `AF_INET6`/`sockaddr_in6`/`IPV6_*`, `MSG_NOSIGNAL`, `MSG_DONTWAIT`, `MSG_WAITALL`, `MSG_TRUNC`, `SCM_RIGHTS`/`SCM_CREDENTIALS`, `IP_ADD_MEMBERSHIP` with `struct ip_mreq`/`ip_mreqn`, and `SO_RCVTIMEO`/`SO_SNDTIMEO`. Configure scripts and `#ifdef`s pick these paths, and they fail at run time. | `GP/bits/socket.h`, `GP/bits/in.h` |
| G3 | High | **`getaddrinfo` thinks IPv6 is available.** I believe this is the most likely cause of the sshd assertion, but I haven't proven it. The generic `__check_pf` reports both IPv4 and IPv6 as present. The RFC 3484 sort then opens `socket(AF_INET6, SOCK_DGRAM)`, which the kernel turns into an IPv4 UDP socket (A2). The connect fails with EAFNOSUPPORT, and the sort's family asserts see the wrong kind of source address. Fix it in two places: make the kernel return EAFNOSUPPORT for AF_INET6 (A2), and give the add-on its own `check_pf.c` that reports IPv4 only until IPv6 exists. | glibc `sysdeps/posix/getaddrinfo.c` (already patched at `:2064`), `inet/check_pf.c` |
| G4 | Medium | **ENOSYS stubs:** `if_nametoindex`, `if_indextoname`, `if_nameindex`, `if_freenameindex`, `sendfile`, `sendfile64`, `getipv4sourcefilter`, `setipv4sourcefilter`, `getsourcefilter`, `setsourcefilter`, `sethostname` and `setdomainname`. `if_nametoindex` is used by curl, libuv, Python's `socket.if_nameindex`, Node and Go. `sendfile` is used by web servers and `cp`-like tools, which usually fall back. | link warnings in `libc.so.2` |
| G5 | Medium | **Missing outright:** `accept4`, `SOCK_CLOEXEC`/`SOCK_NONBLOCK`, `recvmmsg`/`sendmmsg`, `epoll_*`, `eventfd`, `getaddrinfo_a` (no `libanl`), and a native `poll` (it's emulated over select). The POSIX audit covers these in more detail. | `nm -D libc.so.2` |
| G6 | Medium | **`getsockopt` never updates `*optlen`** because of the kernel signature (A5). Python's `getsockopt(level, opt, buflen)`, Java and others read the length back. | `GP/syscalls.list` → kernel `sys_getsockopt(int nOptLen)` |
| G7 | Medium | **`socketpair()` is loopback TCP** (bound to 127.0.0.1, and it checks the peer's port). It's SOCK_STREAM only (SOCK_DGRAM fails with EAFNOSUPPORT), it can't pass fds, and it isn't atomic. A local process can race the `accept()`; it gets caught but fails the call. That's acceptable until AF_UNIX exists. | `GP/socketpair.c` |
| G8 | Low | `sendto.c`/`recvfrom.c` don't initialise `msg_control`, `msg_controllen` or `msg_flags`, and `recv.c` passes a dummy `sockaddr`. That's harmless today because the kernel ignores `msg_control`, but it'll break the day SCM_RIGHTS is added. | `GP/sendto.c`, `GP/recvfrom.c`, `GP/recv.c` |
| G9 | Low | `getifaddrs` is glibc's generic SIOCGIFCONF version. It gives IPv4 address, netmask, broadcast and flags per interface, but no AF_PACKET (MAC address) entries. It should work against `interface.c:get_interface_list`, but I didn't test it. | glibc `sysdeps/gnu/ifaddrs.c` |

### Configuration files (image)

| # | Severity | Finding | Where |
|---|---|---|---|
| F1 | Medium | **The image has no `/etc/services`, `/etc/protocols` or `/etc/nsswitch.conf`.** Without `services`, `getservbyname("http","tcp")` fails, and so does `getaddrinfo(host, "https", …)`, which name-based tools use (wget/curl URLs with schemes, Perl, Python `socket.getservbyname`). Without `protocols`, `getprotobyname("tcp")` fails (Perl IO::Socket, some ports). Without `nsswitch.conf`, glibc's built-in default for hosts is `dns [!UNAVAIL=return] files`. DNS is asked first, and `/etc/hosts` is used only when DNS is unreachable, so lookups of local names are slow and can return the wrong answer. Ship `hosts: files dns`. | `Syllable/system/image/etc/` |
| F2 | Low | **`dhcpc` opens `/etc/resolv.conf` with `O_CREAT \| O_WRONLY`, without `O_TRUNC` and without a mode.** A shorter new file leaves the tail of the old one, and the permissions are random. The shipped default has `domain example.net` / `nameserver 192.168.0.1`, which is wrong under QEMU (10.0.2.3). It never writes `search`/`domain` either. | `apps/utils/dhcpc/interface.c:263-286`, `image/etc/resolv.conf` |

## What works (verified by reading)

- **TCP:** the full state machine including TIME_WAIT, retransmit with slow start and congestion avoidance (`tcp_input.c:1768-1775`), RTT estimation, out-of-order reassembly (`tcp_insert_fragment`), receiver silly-window avoidance, keepalive, non-blocking connect with EINPROGRESS and select writability, FIONREAD, SO_ERROR, SO_KEEPALIVE and MSG_PEEK.
- **UDP:** connected and unconnected sockets, best-match endpoint dispatch, SO_BINDTODEVICE (which dhcpc uses), autobind on first send, and EMSGSIZE for anything over 64 KB.
- **IP:** fragmentation on send and reassembly on receive, loopback through a kernel thread, a route table with cloned host routes, and `SIOCGIFCONF`/`SIOCG/SIFADDR`/`NETMASK`/`BRDADDR`/`MTU`/`FLAGS` (done properly with `memcpy_*_user`).
- **Drivers:** 16 Ethernet drivers plus TUN. rtl8139 is the one tested under QEMU.

## Prioritized roadmap

Effort is a rough guess: **S** < 1 day, **M** a few days, **L** a week or more. "Unblocks" says which software benefits.

### P0: Safety (do first; small, local changes)

| # | Work | Effort | Unblocks |
|---|---|---|---|
| P0-1 | **Validate and copy user memory in every socket syscall** (A1). Copy the `sockaddr` in with `memcpy_from_user` after checking the length. Copy the `msghdr` and iovec array into kernel memory and run `verify_mem_area` on each `iov_base`. Do the same for accept, getsockname and getpeername, and copy `rtentry` in for the route ioctls. The cleanest way is to convert everything in `socket.c`'s `sys_*` wrappers, so the protocols only ever see kernel memory. | M | Closes a local root hole and stops kernel panics on bad pointers. |
| P0-2 | **Validate incoming packets** (B1, B2, C1, D6). In `ip_in`: version 4, IHL ≥ 5, header length ≤ total length ≤ received length, then trim `pb_nSize` to the total length. In UDP: `8 ≤ udp_nSize ≤ IP payload length`. In TCP: data offset ≥ 5 and ≤ the segment length, plus the fixed option end. Replace the odd-byte pad writes with a checksum routine that adds the last byte without writing it. | S–M | Stops remote kernel crashes and data leaks. |
| P0-3 | **Rewrite SO_SNDBUF/SO_RCVBUF** (D1). Clamp to [4 KB, 1 MB]. Refuse to shrink while data is queued, or linearise the ring before copying. Copy `min(old,new)` bytes and remap the indices. | S | Closes a kernel heap overflow; curl, Python and others can set buffer sizes. |
| P0-4 | **Permission checks** (A3, E1): euid 0 for SOCK_RAW, for interface setters, for route add and delete, and for binding ports below 1024. | S | Basic multi-user security. |
| P0-5 | **Reject unknown address families in `socket()`** (A2): EAFNOSUPPORT for everything except AF_INET (and AF_UNIX once it exists). Add a Syllable `check_pf.c` that reports IPv4 only (G3). | S | Fixes the getaddrinfo IPv6 assert (probably), sshd without `AddressFamily inet`, and configure probes. |

### P1: Correct behaviour (things apps trip over today)

| # | Work | Effort | Unblocks |
|---|---|---|---|
| P1-1 | **Make `connect()` report failure properly** (D2): return `-tcb_nError` after waking, map RST to ECONNREFUSED and the timeout to ETIMEDOUT, and return EINTR on a signal. Keep the address and port from an earlier `bind()` (D4). | S | Every client: errors show up at connect time, where programs expect them. |
| P1-2 | **Port management** (D3, D4): EADDRINUSE on duplicate binds; SO_REUSEADDR so TIME_WAIT ports can be reused; a shared, randomised ephemeral range (49152–65535) that skips ports in use; a lock around the hash. | M | Servers restart cleanly, and you can't start two servers on one port by accident. |
| P1-3 | **Socket address fixes** (D5, C5): `htons` in getpeername, set `*namelen`, zero `sin_zero`; add UDP getpeername, getsockopt(SO_ERROR) and shutdown. | S | sshd and logging, libuv, asyncio. |
| P1-4 | **Socket options** (A4, A5, A8, D11): SO_RCVTIMEO and SO_SNDTIMEO (the blocking waits already take timeouts), SO_LINGER (at least l_onoff=0 and the zero-linger RST), honour IP_TTL and IP_TOS, a `getsockopt` that writes back `*optlen` (new syscall argument type; glibc already passes a pointer), and real SOL_TCP getters. | M | DNS clients, Java, Go, Python. |
| P1-5 | **Flags** (D9, C3, C4, A6): MSG_DONTWAIT (TCP, UDP and raw), MSG_NOSIGNAL (skip `sys_kill`), MSG_WAITALL, MSG_PEEK and MSG_TRUNC for UDP, FIONBIO in `sk_ioctl`, and UDP FIONREAD returning the size of the next datagram. | S–M | curl, libuv/Node, nginx-style servers, Python. |
| P1-6 | **ICMP** (B3–B6): handle destination-unreachable (wake TCP connects with ECONNREFUSED/EHOSTUNREACH, set SO_ERROR on connected UDP), handle frag-needed (lower the route MTU, or stop setting DF), send port-unreachable for closed UDP ports with the original header attached, and fix the ECHOREPLY leak. | M | Fast failure for dead hosts, traceroute, VPN/PPPoE paths. |
| P1-7 | **Listen backlog** (D8): treat 0 as 1, cap at SOMAXCONN (128), and silently drop the SYN when the queue is full instead of sending a RST. | S | Servers under load. |
| P1-8 | **glibc fixes** (G1, G6, G8): NULL-safe `recvfrom`, initialise `msg_control`/`msg_flags`, and a getsockopt wrapper that fits the new kernel ABI. | S | Programs that pass NULL to recvfrom. |
| P1-9 | **Interface indexes** (E3, G4): add `SIOCGIFINDEX`/`SIOCGIFNAME` to `netif_ioctl` (the interface already has `ni_nIndex`) and an add-on `if_index.c` that uses them. | S | curl, libuv, Python, Node, Go. |
| P1-10 | **Image config** (F1, F2): ship `/etc/services`, `/etc/protocols` and `/etc/nsswitch.conf` (`hosts: files dns`); fix dhcpc to use `O_TRUNC` with mode 0644 and write `search`; set the default resolv.conf to QEMU's 10.0.2.3 or leave it empty. | S | Service-name lookups, local name resolution. |

### P2: Features (bigger pieces)

| # | Work | Effort | Unblocks |
|---|---|---|---|
| P2-1 | **AF_UNIX** (stream and datagram, filesystem names, real `socketpair`) **with SCM_RIGHTS**. Same as POSIX audit P0-2; the socket layer's `sk_psOps` table makes it a new protocol module. | L | sshd privsep and reexec, ssh-agent, Neovim RPC, Python multiprocessing, dbus, tmux. |
| P2-2 | **TCP performance** (D7, D10): window scaling, SACK, timestamps, fast retransmit and recovery (NewReno), 128–256 KB default buffers with autotuning, and an RFC 6528 ISN once there's a real entropy source (POSIX audit P0-1). | L | git clone, package downloads, Webster. |
| P2-3 | **accept4, SOCK_CLOEXEC/SOCK_NONBLOCK, a native poll syscall**, and an event mechanism (an epoll subset or kqueue). | M–L | libuv/Node, modern servers, Rust/Go runtimes. |
| P2-4 | **Drivers: e1000/e1000e and virtio-net.** Port from the BSD or Linux 2.4-era drivers, as the existing ones were. | M each | Default NICs in QEMU/VirtualBox/VMware, and faster VM networking than rtl8139. |
| P2-5 | **Multicast:** IGMPv2, IP_ADD/DROP_MEMBERSHIP, IP_MULTICAST_IF/TTL/LOOP, Ethernet multicast filters (SIOCADDMULTI is already defined), and IP_PKTINFO. | M | mDNS/Avahi, SSDP, some games. |
| P2-6 | **IPv6.** It touches IP, ICMPv6/ND, TCP, UDP, routing, every sockaddr path and glibc. | L+ | sshd defaults, curl and git over IPv6, Python `has_ipv6`. |
| P2-7 | Real `sendfile`, multiple addresses per interface, a `lo` name (or alias), and AF_PACKET entries in getifaddrs. | M | Web servers, network tools. |

### P3: Hardening and cleanup

Limits on fragment queues and the RX queue (B7, E5); ARP hardening (E4); atomic IP ID (B9); a non-Ethernet `ip_in` entry point (B8); the raw-socket FIXMEs (E2); a 60 s TIME_WAIT; SYN cookies; checking `kmalloc` results (D12); and OOB/exceptfds support if anyone needs it.

## Suggested order

1. **P0-1 to P0-5 together, as one "net hardening" branch.** All five are in `socket.c`, `ip.c`, `udp.c` and `tcp.c`, none of them changes the ABI, and they close the crash and root holes.
2. **P1-1, P1-2, P1-3 and P1-7** (TCP correctness): one more branch in the same files.
3. **P1-5, P1-6, P1-8 to P1-10** (flags, ICMP, glibc and image fixes). P1-4 changes the getsockopt ABI, so it should go out with a glibc rebuild.
4. Then **P2-1 (AF_UNIX)**, which the sshd and Neovim work needs most, followed by **P2-4 (e1000/virtio)** and **P2-2 (TCP performance)**.

A small test program run on the QEMU image after each step would catch regressions: connect to a closed port, bind the same port twice, getpeername, recvfrom with NULL, SO_RCVBUF of 1024, UDP FIONREAD, MSG_NOSIGNAL to a closed peer, and listen(fd, 0). The Linux-AFS test harness in `linux-afs/test/` is a model for how to drive it.
