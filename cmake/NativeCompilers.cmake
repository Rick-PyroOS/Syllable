# Native compilers on the image (next-steps item 3): compilers that run on
# Syllable, built here as Canadian crosses (build Linux, host and target
# Syllable), as Syllable-Build-System/toolchain/scripts/09-native-gcc.sh does.
#
#   /usr/gcc/9.5.0   GCC 9.5.0 (C, C++) + binutils 2.35, gmp/mpfr/mpc/isl
#                    linked in statically
#
# Each set is one prefix, so its gcc finds its own as and ld.  /etc/profile's
# use-gcc switches between the sets.  GCC 9.5.0 needs a C++11 compiler, so these
# are built with the 9.5.0 cross compilers whichever compiler builds the system
# (as gdbserver is).

set(_x95 ${SYLLABLE_TOOLCHAIN_ROOT}/bin/i586-pc-syllable)
set(_with95 CC=${_x95}-gcc CXX=${_x95}-g++)
# The target libraries (libgcc, libstdc++) are built with the cross compiler of
# the same version
set(_for_target95
  CC_FOR_TARGET=${_x95}-gcc CXX_FOR_TARGET=${_x95}-g++ GCC_FOR_TARGET=${_x95}-gcc
  AR_FOR_TARGET=${_x95}-ar AS_FOR_TARGET=${_x95}-as LD_FOR_TARGET=${_x95}-ld
  NM_FOR_TARGET=${_x95}-nm RANLIB_FOR_TARGET=${_x95}-ranlib STRIP_FOR_TARGET=${_x95}-strip
  OBJDUMP_FOR_TARGET=${_x95}-objdump)

# ---- gmp, mpfr, mpc, isl: static, only for building GCC ----------------------

syllable_port(gmp
  PORT gmp-5.0.0
  URL https://mirrors.kernel.org/gnu/gmp/gmp-5.0.0.tar.bz2 https://gmplib.org/download/gmp/archive/gmp-5.0.0.tar.bz2
  SHA256 f089fd0e5c95a07566532952035b090125b2530de2b024aeecdf23c2b787984f
  DEST sysroot CONFIGURE ${_with95}
  BYPRODUCTS ${SYLLABLE_STAGING}/lib/libgmp.a)

syllable_port(mpfr
  PORT mpfr-3.1.0
  URL https://www.mpfr.org/mpfr-3.1.0/mpfr-3.1.0.tar.gz https://mirrors.kernel.org/gnu/mpfr/mpfr-3.1.0.tar.gz
  SHA256 54a3cb110f06f5fb7eb0bc1c8f17b04504d32aef7940ff150e270ba3cb5c58c1
  DEST sysroot CONFIGURE ${_with95} --with-gmp=${SYLLABLE_STAGING}
  DEPENDS gmp BYPRODUCTS ${SYLLABLE_STAGING}/lib/libmpfr.a)

syllable_port(mpc
  PORT mpc-1.1.0
  URL https://mirrors.kernel.org/gnu/mpc/mpc-1.1.0.tar.gz https://ftp.gnu.org/gnu/mpc/mpc-1.1.0.tar.gz
  SHA256 6985c538143c1208dcb1ac42cedad6ff52e267b47e5f970183a3e75125b43c2e
  DEST sysroot CONFIGURE ${_with95} --with-gmp=${SYLLABLE_STAGING} --with-mpfr=${SYLLABLE_STAGING}
  DEPENDS mpfr BYPRODUCTS ${SYLLABLE_STAGING}/lib/libmpc.a)

syllable_port(isl
  PORT isl-0.24
  URL https://libisl.sourceforge.io/isl-0.24.tar.bz2 https://gcc.gnu.org/pub/gcc/infrastructure/isl-0.24.tar.bz2
  SHA256 fcf78dd9656c10eb8cf9fbd5f59a0b6b01386205fe1934b3b287a0a1898145c0
  DEST sysroot CONFIGURE ${_with95} --with-gmp-prefix=${SYLLABLE_STAGING}
  DEPENDS gmp BYPRODUCTS ${SYLLABLE_STAGING}/lib/libisl.a)

# ---- GCC 9.5.0 + binutils 2.35 -----------------------------------------------

# No plugins: Syllable's glibc has no libdl
syllable_port(native-binutils95
  PORT Syllable/BinaryUtilities-2.35
  URL https://mirrors.kernel.org/gnu/binutils/binutils-2.35.tar.bz2 https://ftp.gnu.org/gnu/binutils/binutils-2.35.tar.bz2
  SHA256 7d24660f87093670738e58bcc7b7b06f121c0fcb0ca8fc44368d675a5ef9cff7
  PREFIX /usr/gcc/9.5.0
  CONFIGURE --target=i586-pc-syllable ${_with95}
            --disable-werror --disable-shared --disable-plugins --disable-gdb --disable-sim
            --disable-readline --disable-libdecnumber
  MAKE_ARGS MAKEINFO=true)

syllable_port(native-gcc95
  PORT Syllable/gcc-9.5.0
  URL https://mirrors.kernel.org/gnu/gcc/gcc-9.5.0/gcc-9.5.0.tar.gz https://ftp.gnu.org/gnu/gcc/gcc-9.5.0/gcc-9.5.0.tar.gz
  SHA256 15b34072105272a3eb37f6927409f7ce9aa0dd1498efebc35f851d6e6f029a4d
  PREFIX /usr/gcc/9.5.0
  # The same options as the cross compiler (toolchain/scripts/gcc-common.sh), so
  # code built here and in the build container is the same
  CONFIGURE --target=i586-pc-syllable ${_with95} ${_for_target95}
            --with-gmp=${SYLLABLE_STAGING} --with-mpfr=${SYLLABLE_STAGING}
            --with-mpc=${SYLLABLE_STAGING} --with-isl=${SYLLABLE_STAGING}
            --with-arch=i586 --enable-languages=c,c++ --enable-shared
            --enable-threads=syllable --enable-sjlj-exceptions
            --with-default-libstdcxx-abi=gcc4-compatible --disable-initfini-array
            --disable-tls --enable-clocale=generic --disable-libstdcxx-pch
            --disable-multilib --disable-bootstrap --disable-lto --disable-plugin
            --disable-libssp --disable-libquadmath --disable-libgomp --disable-libatomic
            --disable-libitm --disable-libsanitizer --disable-libvtv --disable-libcc1
  MAKE_ARGS MAKEINFO=true
  DEPENDS gmp mpfr mpc isl native-binutils95)
