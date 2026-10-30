# GCC Upgrade

## Description

We are upgrading gcc.  This will only be the 32bit version of GCC as we don't support 64bit as of yet

## Tools Needed:
NOTE: All ports will live in http://github.com/Rick-PyroOS/Syllable-Ports for now
1.  [x] Autoconfigure - 2.69
2.  [x] Automake - 1.15.1
3.  [x] Flex - 2.5.4 ?
4.  [x] Binutils - 2.35 (for LTO if needed)
5.  [x] Make - 3.80
6.  [x] M4 - 1.4.20
7.  [x] GMP - 5.0.0?
8.  [x] MPFR - 3.1.0?
9.  [x] MPC - 1.1.0?
10. [x] zstd - 1.0.0?
11. [x] isl - 0.24
12. [x] bison - 3.5.1

## Changes to syllable.h

The file syllable.h is not compatible with 9.5.0 / 15...  

1. TARGET_VERSION - modern gcc is deprecated. Our definition uses
fprintf but modern GCC headers should not contain executable code
2. We are missing TARGET_OS_CPP_BUILTINS - prior versions of relied on CPP spec to define the system macros, but modern GCC requires TARGET_OS_CPP_BUILTINS to include __sylalble__ as well as __SYLLABLE__  
3.   

## Changes to tmake_file



## Version upgrade path

GCC 4.1.2 -> GCC 9.5 -> 15.2.0

We will need to use:

--disable-bootstrap
--enable-languages=c,c++
--with-arch=i586
--enable-sjlj-exceptions
--enable-shared
--enable-threads
--with-system-zlib
--disable-libstdcxx-pch
--disable-lto


