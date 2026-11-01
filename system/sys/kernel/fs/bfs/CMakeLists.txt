# BeOS file system (read-only)
syllable_driver(bfs
  SOURCES BlockAllocator.cpp BPlusTree.cpp BufferPool.cpp Debug.cpp Index.cpp Inode.cpp
          Journal.cpp Utility.cpp Volume.cpp kernel_interface.cpp
  DEFINES __LOADABLE__ UNSAFE_GET_VNODE
  OPTIONS -fno-strict-aliasing -Wno-multichar -fno-exceptions
  INSTALL system/drivers/fs)
# links libgcc, as the Makefile did
target_link_options(bfs PRIVATE -lgcc)
