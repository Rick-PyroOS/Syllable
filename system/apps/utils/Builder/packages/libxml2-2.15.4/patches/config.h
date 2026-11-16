/* config.h for libxml2 2.15.4 on Syllable (the port's Makefile, in place of
 * configure): glibc 2.7, built without threads.
 *
 * No getentropy() (glibc 2.7): the hash seed comes from the time and pid.
 * No mmap() of files: unreliable on Syllable (posix audit P0-6).  No glob()
 * (only the tests use it), no modules (dlopen is the tree's libdl, built
 * after the ports), no readline (xmllint isn't built). */

#define HAVE_DECL_GETENTROPY 0
#define HAVE_DECL_GLOB 0
#define HAVE_DECL_MMAP 0

#define HAVE_FUNC_ATTRIBUTE_DESTRUCTOR 1
#define HAVE_STDINT_H 1

#define XML_SYSCONFDIR "/etc"
