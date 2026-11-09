# git for Syllable (cmake/ports/50-git.cmake copies this in as config.mak).
# No source changes to git: every Syllable setting is here.  The build tree's
# paths (CURLDIR, OPENSSLDIR, ZLIB_PATH, PTHREAD_LIBS), prefix and uname_* come
# on make's command line.

TOOLS = i586-pc-syllable
CC = $(TOOLS)-gcc
AR = $(TOOLS)-ar
STRIP = $(TOOLS)-strip
CFLAGS = -O2 -g
# GCC 9.5.0's libgcc linked in: the system's own is 4.1.2's (as cURL)
LDFLAGS = -static-libgcc -L$(OPENSSLDIR)/lib -Wl,-rpath-link,$(OPENSSLDIR)/lib
gitexecdir = libexec/git-core

# Syllable's mmap() can't map a file privately and ignores the offset
# (kernel vfs.c sys_mmap): read files with pread() instead.
NO_MMAP = YesPlease
# No link(): write loose objects with rename().
OBJECT_CREATION_USES_RENAMES = YesPlease
# mkdir("dir/") fails with EINVAL, not EEXIST (kernel vfs.c do_mkdir: the
# trailing slash leaves an empty name): git strips the slash first.
MKDIR_WO_TRAILING_SLASH = YesPlease
NO_INSTALL_HARDLINKS = YesPlease
INSTALL_SYMLINKS = YesPlease

NO_TCLTK = YesPlease
NO_PERL = YesPlease
NO_PYTHON = YesPlease
NO_GETTEXT = YesPlease
NO_EXPAT = YesPlease
NO_IPV6 = YesPlease
NO_SVN_TESTS = YesPlease
NO_STRLCPY = YesPlease

CURL_LDFLAGS = -lcurl -L$(OPENSSLDIR)/lib -lssl -lcrypto -L$(ZLIB_PATH)/lib -lz
# Syllable's linker needs the libraries' run-time paths spelled out
CC_LD_DYNPATH = -Wl,-rpath-link,

# Syllable's <pthread.h> brings in <atheos/semaphore.h>, whose lock_mutex()
# and unlock_mutex() clash with index-pack's own.  git uses none of its calls:
# skip it there (its include guard), keeping the <syllable/stddef.h> it would
# have brought (OS_NAME_LENGTH, for <pthread.h>).
builtin/index-pack.o: EXTRA_CPPFLAGS += -D__F_ATHEOS_SEMAPHORES_H__ -include syllable/stddef.h
