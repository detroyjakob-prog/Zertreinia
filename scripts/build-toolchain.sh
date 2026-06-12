#!/usr/bin/env bash
# ============================================================================
# Build an i686-elf cross compiler (binutils + GCC) into ~/opt/cross.
#
# You only need to run this ONCE. Afterwards add it to your PATH:
#     export PATH="$HOME/opt/cross/bin:$PATH"
#
# Prerequisites (Ubuntu/Debian):
#     sudo apt install build-essential bison flex libgmp3-dev libmpc-dev \
#                      libmpfr-dev texinfo
# ============================================================================
set -euo pipefail

# Versions known to build cleanly together. Bump as you like.
BINUTILS_VERSION="2.42"
GCC_VERSION="13.2.0"

export PREFIX="$HOME/opt/cross"
export TARGET="i686-elf"
export PATH="$PREFIX/bin:$PATH"

SRC="$HOME/src"
JOBS="$(nproc)"

mkdir -p "$SRC" "$PREFIX"
cd "$SRC"

echo ">> Downloading sources..."
[ -f "binutils-$BINUTILS_VERSION.tar.gz" ] || \
    wget "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz"
[ -f "gcc-$GCC_VERSION.tar.gz" ] || \
    wget "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz"

tar -xf "binutils-$BINUTILS_VERSION.tar.gz"
tar -xf "gcc-$GCC_VERSION.tar.gz"

echo ">> Building binutils..."
rm -rf build-binutils && mkdir build-binutils && cd build-binutils
../"binutils-$BINUTILS_VERSION"/configure \
    --target="$TARGET" --prefix="$PREFIX" \
    --with-sysroot --disable-nls --disable-werror
make -j"$JOBS"
make install
cd "$SRC"

echo ">> Building GCC (C only, freestanding)..."
cd "gcc-$GCC_VERSION" && ./contrib/download_prerequisites && cd "$SRC"
rm -rf build-gcc && mkdir build-gcc && cd build-gcc
../"gcc-$GCC_VERSION"/configure \
    --target="$TARGET" --prefix="$PREFIX" \
    --disable-nls --enable-languages=c --without-headers
make -j"$JOBS" all-gcc
make -j"$JOBS" all-target-libgcc
make install-gcc
make install-target-libgcc

echo
echo ">> Done. Add this to your shell profile:"
echo "       export PATH=\"$PREFIX/bin:\$PATH\""
echo ">> Verify with:  $TARGET-gcc --version"
