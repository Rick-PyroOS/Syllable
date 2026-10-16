/* Forced into every Syllable source the host tools compile (-include):
 * Syllable's libc headers pull <string.h> in from everywhere; glibc's don't. */
#include <string.h>
#include <strings.h>
