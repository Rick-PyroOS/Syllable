# Google Test and Google Mock 1.8.1 for Syllable.  The tarball has no
# Makefile at its top, only CMake and autotools builds; this one builds the
# four static libraries upstream's CMake makes (libgtest, libgtest_main,
# libgmock, libgmock_main) from the "fused" sources (gtest-all.cc,
# gmock-all.cc), with the system's GCC 4.1.2 (C++98), and installs them with
# their headers (include/gtest, include/gmock) under $(prefix).
#
# Static only, as upstream recommends: a test program links its own copy, so
# it is built with the same compiler and flags as the code it tests.
#
# Syllable is not one of Google Test's known platforms (no GTEST_OS_*), so it
# takes the generic POSIX path: no death tests and no thread safety
# (GTEST_IS_THREADSAFE 0), which means assertions are made from one thread,
# the one running the tests.  GTEST_HAS_PTHREAD=0 says so outright.
#
#   make CC=i586-pc-syllable-gcc CXX=i586-pc-syllable-g++ AR=i586-pc-syllable-ar RANLIB=i586-pc-syllable-ranlib
#   make install prefix=/path/to/dest/system/resources/googletest/1.8.1

CXX    = g++
AR     = ar
RANLIB = ranlib
prefix = /usr/local

GTEST = googletest
GMOCK = googlemock

CXXFLAGS = -O2
ALL_CXXFLAGS = -fPIC -Wall -fexceptions -DGTEST_HAS_PTHREAD=0 \
               -I$(GTEST)/include -I$(GTEST) -I$(GMOCK)/include -I$(GMOCK) $(CXXFLAGS)

LIBS = libgtest.a libgtest_main.a libgmock.a libgmock_main.a

all: $(LIBS) gtest_sample

# GCC 4.1.2 misreads the comma in a template-id in a member function's
# default argument (GCC bug 57): gtest-internal.h's two
#   type_names = GenerateNames<DefaultNameGenerator, Types>()
# Parenthesised, they parse.  The header is fixed in place, so the installed
# copy (which programs with typed tests include) is fixed too.
GCC41_FIX = $(GTEST)/include/gtest/internal/gtest-internal.h
gcc41-fixed: $(GCC41_FIX)
	sed -i 's/^\( *\)GenerateNames<DefaultNameGenerator, Types>())/\1(GenerateNames<DefaultNameGenerator, Types>()))/' $(GCC41_FIX)
	touch $@

gtest-all.o: $(GTEST)/src/gtest-all.cc gcc41-fixed
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@
gtest_main.o: $(GTEST)/src/gtest_main.cc gcc41-fixed
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@
gmock-all.o: $(GMOCK)/src/gmock-all.cc gcc41-fixed
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@
gmock_main.o: $(GMOCK)/src/gmock_main.cc gcc41-fixed
	$(CXX) $(ALL_CXXFLAGS) -c $< -o $@

libgtest.a: gtest-all.o
	$(AR) rcs $@ gtest-all.o
libgtest_main.a: gtest_main.o
	$(AR) rcs $@ gtest_main.o
# As upstream's CMake: libgmock carries Google Test too, so -lgmock alone links
libgmock.a: gmock-all.o gtest-all.o
	$(AR) rcs $@ gmock-all.o gtest-all.o
libgmock_main.a: gmock_main.o
	$(AR) rcs $@ gmock_main.o

# Upstream's first sample (sample1 and its tests), linked with gtest_main: a
# quick check on Syllable that the framework runs (./gtest_sample).  Not
# installed.
gtest_sample: $(GTEST)/samples/sample1.cc $(GTEST)/samples/sample1_unittest.cc libgtest.a libgtest_main.a
	$(CXX) $(ALL_CXXFLAGS) -o $@ $(GTEST)/samples/sample1.cc $(GTEST)/samples/sample1_unittest.cc \
	  libgtest_main.a libgtest.a

install: all
	mkdir -p $(DESTDIR)$(prefix)/lib $(DESTDIR)$(prefix)/include
	cp $(LIBS) $(DESTDIR)$(prefix)/lib/
	cp -R $(GTEST)/include/gtest $(GMOCK)/include/gmock $(DESTDIR)$(prefix)/include/

.PHONY: all install
