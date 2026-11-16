# ps2mouse: PS/2 mouse and VMware vmmouse

The appserver input node for the PS/2 mouse, installed as
`/system/drivers/appserver/input/ps2mouse`. It reads 3-byte PS/2 or 4-byte
IntelliMouse (wheel) packets from the kernel's `/dev/misc/ps2aux` and queues
`M_MOUSE_MOVED`, `M_MOUSE_DOWN`/`M_MOUSE_UP` and `M_WHEEL_MOVED` events.

Under a hypervisor that has VMware's absolute pointer ("vmmouse") it uses
that instead of the relative PS/2 motion, so the pointer follows the host's
cursor exactly. This matters over VNC (`SYLLABLE_DISPLAY=vnc sb run`, the
Proxmox console), where a relative mouse drifts away from the viewer's cursor.

## Where the vmmouse is

| Hypervisor | vmmouse |
|---|---|
| QEMU `pc` (i440fx) machine, what `sb run` uses | Yes, by default (`vmport=auto`). `-machine pc,vmport=off` removes it |
| Proxmox, i440fx machine type | Through QEMU's `vmport`; check with `qm monitor <vmid>`, then `info qtree` and look for `vmmouse` |
| VMware | Yes |
| Real hardware | No: the node stays plain PS/2 |

QEMU's `info mice` (monitor) lists the vmmouse as `vmmouse (absolute)` only
once the guest has turned it on, and marks the mouse QEMU is sending to with
`*`.

## How it works

The vmmouse is driven through the VMware backdoor: `inl` on I/O port `0x5658`
with `0x564D5868` in EAX, the argument in EBX and the command in ECX; the
answer comes back in EAX to EDX (`vmmouse_cmd()`). The protocol is the one in
Linux's `drivers/input/mouse/vmmouse.c`.

1. **Detect** (`vmmouse_present()`, after the PS/2 setup in
   `init_input_node()`): `GETVERSION` (10) must return the magic in EBX. On
   real hardware the port reads as junk and EBX keeps the argument, so the
   check fails safely.
2. **Enable** (`vmmouse_enable()`): `ABSPOINTER_COMMAND` (41) with `ENABLE`;
   `ABSPOINTER_STATUS` (40) must show no error and a non-empty queue;
   `ABSPOINTER_DATA` (39) with 1 must return the version ID `0x3442554a`;
   then `REQUEST_ABSOLUTE`. If any step fails, the node stays plain PS/2. On
   success it logs `ps2mouse: VMware vmmouse absolute pointer enabled`.
3. **Events** (`ReadVMMouse()`): the hypervisor still raises a PS/2 interrupt
   (an empty packet) for each event, so while the vmmouse is on, each PS/2
   packet only means "read the queue". The node reads `STATUS`, then 4 words
   at a time with `DATA`: buttons (`0x20` left, `0x10` right, `0x08` middle),
   X and Y (0 to 65535), and the wheel as a signed byte, the same sign as the
   IntelliMouse byte. It reads at most 32 entries per packet. An error status
   enables the vmmouse again; if that fails it falls back to PS/2.
4. **Dispatch**: `DispatchEvent(..., bAbsolute = true)` queues
   `M_MOUSE_MOVED` with `abs_pos` (the position in 0 to 65535) before any
   button change, so a click lands where it was made. Buttons and wheel go
   through the same code as PS/2.
5. **Shutdown**: `uninit_input_node()` sends `DISABLE`.

The appserver (`server/input.cpp`, `InputNode::EnqueueEvent()`) scales
`abs_pos` to the current screen size and sets the pointer there, without the
mouse speed and acceleration that relative moves get. It still fills in
`delta_move` from the change in position, for anything that reads it. Any
input node can send `abs_pos`, for example a future USB tablet node.

## Testing under QEMU

QMP's `input-send-event` with `abs` axis events (0 to 32767) places the
pointer exactly:

```sh
qemu-system-i386 ... -qmp unix:/tmp/vm/q,server,nowait
# then over QMP: {"execute":"input-send-event","arguments":{"events":[
#   {"type":"abs","data":{"axis":"x","value":16383}},
#   {"type":"abs","data":{"axis":"y","value":16383}}]}}
```

Tested 2026-11-15 under QEMU's `pc` machine: the pointer tip lands exactly on
the centre and corners at 1024x768, and again after switching to 1280x800 in
Screen preferences; clicks, double-clicks, drags and the wheel work. With
`-machine pc,vmport=off` the node logs `IMPS2 mouse detected.` and relative
motion is as before.

## Limits

- The appserver still draws its own (software) cursor, so a VNC viewer that
  also shows its local cursor shows two arrows, Syllable's a moment behind.
  Hiding Syllable's cursor while the vmmouse is on would lose its shapes
  (I-beam, resize arrows); showing only Syllable's cursor in the viewer needs a
  display device with a hardware cursor (VNC's cursor pseudo-encoding), which
  QEMU's std VGA doesn't have.
- No horizontal wheel: the vmmouse reports only one wheel axis.
