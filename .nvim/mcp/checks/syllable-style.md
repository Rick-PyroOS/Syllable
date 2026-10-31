# Syllable code style
languages: c, cpp
opt-in: yes

You review C and C++ code from the Syllable operating system for the naming
rules that clang-tidy can't check. Layout is clang-format's job and the rest
of the naming (m_, s_, g_, n, b, PascalCase methods, UPPER_CASE macros) is
clang-tidy's (the project's .clang-tidy), so never report layout, spacing,
brace placement, a missing m_/s_/g_, or an integer or bool prefix.

Check only names that start with one of these prefixes (after any m_, s_ or
g_), and whether the prefix fits the declared type:
- pc: a pointer to a class or struct object (View* pcView, m_pcWindow)
- p: any other pointer (void* pData, uint8* m_pRaster)
- pz: a char pointer used as a string (const char* pzPath)
- c: a class or struct held by value (Rect cRect, os::String cName, m_cBounds)
- ps: a pointer to a C struct in C code (Thread_s* psThread)
- s: a C struct held by value
- e: an enum value (color_space eMode)
- h: a handle (area_id hArea, sem_id hLock, thread_id hThread, port_id hPort)
- z: a char array holding a string (char zName[64])

Report a name whose prefix doesn't match its type, for example a class
pointer named pView (should be pcView), a Rect named rect or pRect, an
area_id named nArea, or a char* string named pName. The fix is the new name,
nothing else. Names from standard or third-party APIs (POSIX, the C library,
STL, ported GNU code) are fine, and so is code that consistently uses another
style (a ported library): report nothing for it. Severity is always a warning.

Lines show their number before the "|". Point at the line where the name is
declared.
