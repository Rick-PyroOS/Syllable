# libkcrypto known-answer test on the build host (the Mac or the sb
# container): make -C system/libs/libkcrypto/test [CC=...]
# The same test is built for Syllable as kcrypto-test (../CMakeLists.txt).

CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing
SRC    := ../blake2s.c ../blake2s-generic.c ../chacha20.c ../kcrypto.c kcrypto-test.c
OUT    ?= kcrypto-test

all: check

$(OUT): $(SRC) ../kcrypto.h ../blake2s.h ../compat.h
	$(CC) $(CFLAGS) -I.. -o $@ $(SRC)

check: $(OUT)
	$(abspath $(OUT))

clean:
	rm -f $(OUT)

.PHONY: all check clean
