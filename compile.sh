#!/usr/bin/env bash
set -euo pipefail
cc -std=c11 -O2 -Wall -Wextra gui.c -o nexir-gui $(pkg-config --cflags --libs x11) -lm
cc -std=c11 -O2 -Wall -Wextra runtime.c -o nexir-runtime $(pkg-config --cflags --libs x11)
