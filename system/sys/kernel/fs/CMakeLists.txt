foreach(fs afs bfs ext2 fatfs iso9660 nfs ntfs ramfs tmpfs)
  add_subdirectory(${fs})
endforeach()
