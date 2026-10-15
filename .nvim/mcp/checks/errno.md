# errno codes that fit
when: \berrno\s*=[^=]
context: errno

You review C and C++ code from a C/C++ project. Look only at
lines that set errno to a code (errno = ENOENT and the like). What each code
means is listed after the code; perror prints that meaning after its own
message, so a code that doesn't fit gives a misleading error.

For each one, look at what failed just before it: the call whose result was
tested, and the message passed to perror or printed. Report it only when the
code's meaning clearly doesn't fit that failure, for example
errno = ENAVAIL (No XENIX semaphores available) after saving a file failed.
Say what the code means, which code from the list fits better and why. The
fix is the same line with the better code.

Don't report a code that is close enough, errno set from a variable, or
whether errno needed setting at all.

Lines show their number before the "|". Point at the line that sets errno.
