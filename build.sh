#!/usr/bin/env bash

set -e

CC="$(which clang)"

CFLAGS=(
  -std=gnu11 # For MAP_ANONYMOUS and asm
  -Wall
  -Wextra
  -ggdb
  -msse4.1
  -ffast-math
  -O3
  $(pkg-config --cflags alsa)
  $(pkg-config --cflags x11)
  $(pkg-config --cflags xrandr)
  -Wno-gnu-alignof-expression
)

LDFLAGS=(
  $(pkg-config --libs alsa)
  $(pkg-config --libs x11)
  $(pkg-config --libs xrandr)
  -lm
  -pthread
)

DEFINES=(
  -DDEBUG_MODE=1
)

ROOT_DIR="$(cd $(dirname "$0") && pwd)"
SRC_DIR="$ROOT_DIR"/src
BUILD_DIR="$ROOT_DIR"/build

test -d "$SRC_DIR"
test -d "$BUILD_DIR" || mkdir -p "$BUILD_DIR"

pushd "$SRC_DIR" > /dev/null

set -x
"$CC" "${CFLAGS[@]}" "${DEFINES[@]}" linux/main.c -o "$BUILD_DIR"/fonograf "${LDFLAGS[@]}"
set +x

popd > /dev/null
