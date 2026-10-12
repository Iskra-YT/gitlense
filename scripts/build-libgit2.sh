#!/usr/bin/env bash
# Buduje i instaluje libgit2 w prefiksie uzytkownika (~/.local/gitlense-deps).
# Uzywane, gdy w systemie nie ma pakietu libgit2-dev (brak uprawnien roota).
set -euo pipefail

VERSION="${LIBGIT2_VERSION:-1.9.7}"
PREFIX="${LIBGIT2_PREFIX:-$HOME/.local/gitlense-deps}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/libgit2-build.XXXXXX")"
CMAKE_BIN="${CMAKE_BIN:-$(dirname "$(command -v cmake 2>/dev/null || echo /nonexistent)")/cmake}"

if [ ! -x "$CMAKE_BIN" ]; then
    CMAKE_BIN="$(command -v cmake)"
fi

echo "==> libgit2 $VERSION -> $PREFIX (cmake: $CMAKE_BIN)"
cd "$WORK"

git clone --depth 1 --branch "v$VERSION" --recurse-submodules --shallow-submodules \
    https://github.com/libgit2/libgit2.git src

cmake -S src -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTS=OFF \
    -DLIBGIT2_TESTS=OFF \
    -DLIBGIT2_EXAMPLES=OFF \
    -DLIBGIT2_BUILD_CLI=OFF \
    -DLIBGIT2_USE_SSH=OFF \
    -DLIBGIT2_HTTPS=OFF \
    -DUSE_SSH=OFF \
    -DUSE_HTTPS=OFF \
    -DLIBGIT2_USE_BUNDLED_ZLIB=ON \
    -DLIBGIT2_USE_BUNDLED_PCRE=OFF \
    -DLIBGIT2_USE_THREADS=ON

cmake --build build --parallel "$(nproc)"
cmake --install build

echo "==> gotowe: $PREFIX/lib/pkgconfig/libgit2.pc"
rm -rf "$WORK"
