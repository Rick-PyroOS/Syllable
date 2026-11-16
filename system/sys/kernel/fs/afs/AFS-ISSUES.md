# AFS bugs found while testing the Linux driver

These turned up while generating test images with this directory's own code
(built for Linux as `linux-afs/tools/afstool`). Each one was reproduced
against the unmodified sources here. Line numbers refer to the files in this
directory as of commit `00a9b817`.

| # | Problem | Severity | Fix size |
|---|---|---|---|
| 1 | After a crash, the journal can be written outside the log area, over live metadata | **Destroys volumes** | Small (afs.c) |
| 2 | Read-only mounts start journal transactions, then hang, and refuse a dirty journal | Hang | Small to medium |
| 3 | Attributes in the attribute directory aren't listed when an inode has no small-data attributes | Wrong results, AFS_PARANOIA panic | One character |
| 4 | Large writes fail with ENOSPC while there is plenty of free space in small holes | Spurious disk full | Medium |
| 5 | Deleting a large file can make every later mount panic or fall back to read-only | **Volume won't mount** | Medium (stream.c, afs.c) |
| 6 | `/var/log/kernel` (dbterm) grows forever and keeps the journal busy, which sets up 1, 2 and 5 | Triggers the above | Workaround now, logging service later |

---

## 1. The journal escapes the log area after a crash

### What happens

`afs_mount()` takes the log area from the superblock's *current* log start,
not from the log's block run (afs.c:843-845):

```c
psVolume->av_nJournalSize = psSuperBlock->as_nLogSize;
psVolume->av_nLoggStart = psSuperBlock->as_nLogStart;     /* where pending entries begin */
psVolume->av_nLoggEnd = psVolume->av_nLoggStart + psVolume->av_nJournalSize - 1;
```

`as_nLogStart` is the start of the pending entries. On a clean volume that
is also the start of the log area, `as_sLogBlock`, so nothing goes wrong.
After a crash, though, it usually points part way into the log, because the
entries before it were already flushed home. That causes two failures:

- **Replay of a wrapped journal fails.** `afs_replay_log()` wraps at
  `as_nLogStart + size - 1` instead of at the real end of the log. It reads
  blocks past the log and stops with "failed to write back journal block"
  (destination -1). The mount fails, and the volume can't be mounted
  read-write again.
- **When replay succeeds, the damage is permanent.** Replay finishes with
  `as_nLogStart = av_nLoggStart`, which is the same mid-log value. Every
  later mount computes the same shifted log area, and
  `afs_discard_flushed_journal_entries()` resets `as_nLogStart` to it
  (journal.c:880). From then on, each session writes journal blocks from
  that point onward, past the end of the log area, over the root
  directory, inodes and bitmap.

### Reproduced

On a 16 MB volume (log at blocks 36–2083), I imported 800 files and set
`as_nLogStart` to 1500 with no pending blocks, which is the state a
successful replay leaves. Importing 3,000 more files then gave this fsck
result:

```
ERROR: [index-dir]: inode 2085 claims to be inode 5457
ERROR: [delete-me]: inode 2086 claims to be inode 5458
afstool: /: readdir failed: Input/output error
```

All 800 original files were gone.

### Fix

Derive the log area from the log block run, and only use `as_nLogStart` as
the replay position (afs.c, around line 843):

```c
psVolume->av_nJournalSize = psSuperBlock->as_nLogSize;
psVolume->av_nLoggStart = afs_run_to_num( psSuperBlock, &psSuperBlock->as_sLogBlock );
psVolume->av_nLoggEnd = psVolume->av_nLoggStart + psVolume->av_nJournalSize - 1;

if( psSuperBlock->as_nLogStart < psVolume->av_nLoggStart ||
    psSuperBlock->as_nLogStart > psVolume->av_nLoggEnd )
{
	printk( "Error: afs_mount() log start %Ld outside log area\n", psSuperBlock->as_nLogStart );
	nError = -EINVAL;
	goto error4;    /* or reset it if as_nValidLogBlocks == 0 */
}
```

Nothing else needs to change. `afs_replay_log()` already starts reading at
`as_nLogStart`. Its wrap test, the writer's wrap in
`afs_write_transaction_to_log()` and `afs_create_transaction()` all use
`av_nLoggStart`/`av_nLoggEnd`, so they become correct. At the end of
replay, `as_nLogStart = av_nLoggStart` then rewinds to the real log start.
That also repairs a volume that has already drifted, if it has survived so
far.

The Linux driver (`linux-afs/kernel/super.c`, `sylafs_map_journal()`) and
`linux-afs/test/replay_journal.py` already wrap at the end of the log run.

---

## 2. Read-only mounts write, hang, or refuse to mount

### What happens

- **`afs_close()` always starts a transaction** and calls
  `afs_truncate_stream()` to trim preallocation (afs.c:2122). It does this
  even when the file was only read and even on a read-only volume. For files
  with double-indirect blocks the trim produces dirty blocks. On a read-only
  device, the log write then fails, and `afs_wait_for_log_space()` waits
  forever. I hit this simply by reading a 40 MB fragmented file on a
  read-only mount.
- **`afs_empty_delme_dir()` starts a transaction before checking for
  read-only** (afs.c:556). If the delete-me directory isn't empty, the
  `FS_IS_READONLY` branch (around line 606) `break`s out without
  `afs_end_transaction()`, leaking the open transaction.
- **A dirty journal can't be mounted read-only.** `afs_replay_log()` returns
  `-EROFS` (journal.c:491), so after a crash the volume can only be mounted
  read-write.

### Fix

`afs_close()`: read the open mode before freeing the cookie, and only trim
files that were opened for writing on a writable volume:

```c
static int afs_close( void *pVolume, void *pNode, void *pCookie )
{
	AfsVolume_s * psVolume = pVolume;
	AfsInode_s * psInode = ( ( AfsVNode_s * )pNode )->vn_psInode;
	AfsFileCookie_s *psCookie = pCookie;
	bool bWrote = psCookie == NULL || ( psCookie->fc_nOMode & O_RWMASK ) != O_RDONLY;
	int nError = 0;

	if( pCookie != NULL )
		kfree( pCookie );
	if( ( psVolume->av_nFlags & FS_IS_READONLY ) || !bWrote )
		goto notify;
	... existing transaction + afs_truncate_stream() ...
notify:
	... existing INF_STAT_CHANGED handling ...
}
```

`fc_nOMode` is set by both `afs_create()` (afs.c:1320) and `afs_open()`
(afs.c:2104). The index-directory cookie in index.c sets it to `O_RDONLY`.

`afs_empty_delme_dir()`: return 0 at the top when `FS_IS_READONLY` is set,
before `afs_begin_transaction()`. Also add `afs_end_transaction( psVolume,
false )` before the `-EROFS` break, in case other callers reach it.

Dirty journal on a read-only mount: either replay anyway, as long as the
device itself is writable (the journal only rewrites blocks that were
already committed), or keep a block → log-block map in memory and consult
it in `afs_logged_read()` and the cache reads. That map is what the Linux
driver does. The first option is the smaller change.

---

## 3. Attributes not listed when the inode has no small-data attributes

### What happens

`afs_read_attrdir()` (attribs.c:451) switches from small-data entries to
the attribute directory like this:

```c
if( psIterator->aa_nPos > 0 )
{
	nError = bt_find_first_key( psVolume, psAttrInode, &psIterator->aa_sBIterator );
	psIterator->aa_nPos = -1;
}
else
{
	nError = bt_get_next_key( psVolume, psAttrInode, &psIterator->aa_sBIterator );
}
```

If the inode had no small-data entries, `aa_nPos` is still 0. The code then
calls `bt_get_next_key()` on an iterator that was never started
(`bi_nCurNode == -1`), and the B+tree walk is never begun. With
AFS_PARANOIA (on in afs.h) this panics: "bt_load_node() asked to load a
NULL_VAL node!!". Reading the attribute by name still works; only listing
is broken.

This happens to any file whose attributes are all too big for the inode,
for example a single 3 KB attribute.

### Fix

```c
if( psIterator->aa_nPos >= 0 )    /* was: > 0 */
```

`aa_nPos` is 0 or more while small data is being returned, and -1 once the
B+tree walk has started, so `>= 0` means "start the walk now".

---

## 4. ENOSPC with plenty of free space

### What happens

When a file grows past its 12 direct runs, `afs_expand_indirect_range()`
allocates the indirect pointer run through `afs_alloc_meta_blocks()`
(stream.c:364). That asks `afs_alloc_blocks()` for exactly
`BLOCKS_PER_DI_RUN` (4) contiguous blocks, with lengths rounded down to a
multiple of 4 (balloc.c:434, `nMask`). If the volume's free space is all in
holes of 1–3 blocks, which is what remains after deleting many small files,
the allocation fails. The write then returns ENOSPC.

Reproduced on a 16 MB volume with 6,561 free blocks (mostly 2-block holes
from deleted 1 KB files): a 2.5 MB write failed with

```
afs_alloc_blocks() failed to alloc 4 blocks
afs_expand_indirect_range() failed to alloc indirect pointer block
```

The file's direct runs had already taken the larger free extents, so there
was nowhere left for the 4-block pointer run.

### Fix (roughly)

- **Indirect range:** nothing in the format requires the indirect run to be
  4 blocks. `afs_get_stream_blocks()`, the truncate code and GRUB's
  `fsys_afs.c` all loop over `ds_sIndirect.len`. Give
  `afs_alloc_meta_blocks()` a modulo argument. In
  `afs_expand_indirect_range()`, ask for up to 4 blocks with modulo 1 and
  accept whatever length comes back (at least 1). Check that the
  pointer-count calculations in `afs_expand_indirect_range()` use
  `ds_sIndirect.len * nPtrsPerBlk` rather than a fixed 4.
- **Double-indirect range:** here the format does require 4-block runs,
  because the index arithmetic assumes `BLOCKS_PER_DI_RUN`. The practical
  fix is to reserve the pointer runs early. Allocate the indirect (and
  later the double-indirect) pointer run when the file's direct or
  indirect runs are nearly used up, while large extents are still
  available, instead of at the moment they're needed.
- Optional: when `afs_alloc_blocks()` fails only because of the modulo,
  log that, so "disk full" with free space is easier to diagnose.

---

## 5. Deleting a large file can stop every later mount

Worked out from the code; not reproduced yet.

### What happens

When a file is unlinked while it's still open, or the system goes down
before the delete finishes, the inode stays in the delete-me directory.
On the next mount, `afs_empty_delme_dir()` (afs.c:556) frees each one.
It sets the size to 0 and calls `afs_truncate_stream()` inside **one**
transaction.

For a file in the double-indirect range, that transaction collects:

- about 3 of every 4 pointer blocks (`afs_trunc_double_indirect()` writes a
  pointer block back through the log unless it is freed outright, and a
  pointer block maps 512 data blocks), so roughly 3 log blocks per 2 MB;
- every bitmap block that holds one of the file's blocks (one bitmap block
  covers 8 MB with 1 KB blocks). A file grown by many small appends is
  spread across the disk, so this can be most of the bitmap.

The journal is 2048 blocks (afs.c:264). `afs_merge_transactions()` asks
for the transaction's size plus 1/128 of the log, and
`afs_wait_for_log_space()` panics when that exceeds the journal
(journal.c:930: "%d log blocks requested but only %d available"). The only
safety valve is in `afs_truncate_stream()` (stream.c:1232), which splits
the work, but only when more than 256K blocks (256 MB) are being removed
from the double-indirect range. A scattered file of tens to a few hundred
MB is too big for one transaction and too small to be split.

Outcomes, depending on how the transaction fails:

- **Panic during mount.** Same panic on every boot, because the file is
  still in delete-me.
- **Error instead of panic.** `afs_mount()` prints "failed to cleanup
  delete-me directory! Degrading mount to read only" (afs.c:910) and
  continues read-only. Issue 2 then makes the read-only volume hang on the
  first close that wants to trim a file, so it looks like the mount hung.

The same oversized transaction happens at run time when a large closed
file is deleted (`afs_delete_inode()`, afs_inode.c:314, truncates the same
way). If that crashes part way, the file ends up in delete-me and the
mount-time case above takes over.

Smaller problems on the same path:

- The split path calls `afs_checkpoint_journal()` (stream.c:1254) while the
  caller's transaction is still open, and it is skipped for files with
  `INF_LOGGED`.
- `afs_truncate_stream()` ignores the return value of
  `afs_trunc_indirect()` (stream.c:1276).

### Fix (roughly)

- **Bound the transaction by blocks touched, not by file size.** In
  `afs_truncate_stream()`, free the double-indirect range in chunks (for
  example one 4-block pointer run, 2048 data blocks, at a time; or stop when
  the transaction has touched a few hundred blocks). Between chunks write
  the inode (`afs_do_write_inode()`), then `afs_end_transaction( psVolume,
  true )` and `afs_begin_transaction()`. Truncating from the end means
  each committed chunk leaves a valid, shorter file, so a crash in the
  middle just resumes on the next mount. Callers that hold a transaction
  need a flag or a separate entry point so the helper can cycle it.
- **Don't let one bad entry take the volume down.** In
  `afs_empty_delme_dir()`, if freeing an inode fails, log it, end the
  transaction, and move to the next entry instead of returning an error.
  Keep the mount read-write; the leaked blocks only cost space until a
  later mount or fsck frees them.
- Lower or remove the 256K-block threshold, and make the split apply to
  `INF_LOGGED` files too.

---

## 6. `/var/log/kernel` and dbterm

This one isn't a bug in afs itself. It's the workload that most often
leads to 1 and 5, so it's documented on its own until there is a proper
logging service.

### What happens

`start.sh:16` runs `/system/programs/dbterm 8 >> /var/log/kernel &`.

- **The file is open for the whole session and never rotated.** It only
  grows. Other scripts also append to it (`network-start.sh:2`), and each
  of those closes trims the file's preallocation through `afs_close()`,
  which is another transaction.
- **Every append is a journaled metadata change** (inode size, bitmap,
  pointer blocks). Syllable rarely shuts down with the file closed, so
  after a power-off or crash the journal nearly always has pending entries
  and has often wrapped. Replaying a wrapped journal is exactly what
  issue 1 gets wrong: replay reads past the log area and the mount fails
  with "failed to write back journal block" / "afs_mount() failed to
  replay journal". If replay does succeed, the log area is left shifted
  and later sessions overwrite metadata.
- **A dirty journal can't be mounted read-only** (issue 2), so mounting
  the disk read-only from another system also fails after a crash.
- **Deleting the log to free space triggers issue 5.** Small appends
  spread the file across the disk, so its truncate transaction is
  unusually large for its size.
- **There is a size cap.** With 1 KB blocks the double-indirect range
  holds 4 × 128 × 4 × 128 × 4 = 1,048,576 blocks, so the file stops
  growing a little past 1 GB. `afs_expand_doubleindirect_range()` prints
  "max file size reached" and writes fail. This doesn't affect mounting.

### Workaround for now

Rotate at boot, before dbterm starts, so the file that eventually gets
deleted is always small:

```sh
# start.sh, before the dbterm line
if [ -e /var/log/kernel ]; then
	rm -f /var/log/kernel.old
	mv /var/log/kernel /var/log/kernel.old
fi
/system/programs/dbterm 8 >> /var/log/kernel &
```

`mv` only changes directory entries, so it's cheap however big the file
is. The `rm` frees the previous boot's log, which is one session's worth.
If an existing log is already very large, delete it once from a system
that has fixes 1 and 5, or accept that the first boot after this change
does the big delete.

### Longer term

A logging service that owns the log files: rotate by size, keep a few
generations, and close and reopen files on rotation so preallocation is
trimmed and the journal can drain. It should also close its files at
shutdown so the volume unmounts clean.

---

## Which one am I hitting?

The boot log tells them apart:

| Messages | Cause |
|---|---|
| `afs_replay_log() N log blocks from …`, then `failed to write back journal block` or `afs_mount() failed to replay journal` | 1 (wrapped journal) |
| `afs_replay_log() Trying to replay log on read-only FS` | 2 (dirty journal, read-only mount) |
| `Superblock has more valid journal blocks … than the total journal size` | Superblock damage, usually after 1 |
| `afs_empty_delme_dir() delete inode … (size=…)`, then `… log blocks requested but only 2048 available` | 5 (panic) |
| `failed to cleanup delete-me directory! Degrading mount to read only`, then a hang | 5, then 2 |

---

## How to reproduce

Build `linux-afs/tools/afstool` (see its README). Useful pieces:

- `AFSTOOL_CRASH=1 afstool IMG <write command>` stops after the journal is
  written, without unmounting. That produces a dirty journal like a crash.
- `afstool IMG replay` mounts read-write once, so Syllable replays the
  journal.
- `afstool IMG interleave 4096 a /a b /b` writes two files in turns, which
  fragments them into indirect and double-indirect blocks.
- `afstool IMG fsck --strict` is an independent checker, not Syllable's
  code.
- `linux-afs/test/make-images.sh` builds the images used above, including a
  crashed volume with a wrapped journal.
