# AFS, Syllable's native filesystem
syllable_driver(afs
  SOURCES afs.c balloc.c afs_inode.c attribs.c index.c journal.c stream.c btree.c bt_delete.c
  DEFINES __LOADABLE__
  OPTIONS -fno-strict-aliasing
  INSTALL system/drivers/fs)
