#!/usr/bin/env bash
# Compile-only native port core. This object is not linked into Explore or installed.
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build/original
"${CC_NATIVE_WINDOWS:-x86_64-w64-mingw32-gcc-posix}" \
  -std=c11 -O2 -Wall -Wextra -Werror -ffp-contract=off -fno-fast-math \
  -fno-ident -c src/original_scalar.c -o build/original/original_scalar.obj
