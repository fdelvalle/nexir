CC ?= cc
CFLAGS ?= -O2 -g
WARNINGS = -std=c11 -Wall -Wextra -Wpedantic
X11_CFLAGS ?= $(shell pkg-config --cflags x11)
X11_LIBS ?= $(shell pkg-config --libs x11)
CORE = src/parser.c src/engine.c
HEADERS = src/nxg.h src/engine.h
GAME ?= build/crossing.nxg

.PHONY: all examples test sanitize run smoke clean
all: nexir-runtime nexir-gui examples

nexir-runtime: src/main.c src/parser.c src/nxg.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) $(LDFLAGS) src/main.c src/parser.c -o $@

nexir-gui: src/main.c src/x11.c src/frontend.h $(CORE) $(HEADERS)
	$(CC) $(CPPFLAGS) $(X11_CFLAGS) $(CFLAGS) $(WARNINGS) -DNX_X11 $(LDFLAGS) src/main.c src/x11.c $(CORE) $(X11_LIBS) -o $@

examples: build/crossing.nxg build/bounce.nxg

build/crossing.nxg build/bounce.nxg &: examples/build.py
	python3 examples/build.py build

build/test-engine: tests/engine.c $(CORE) $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -Isrc $(LDFLAGS) tests/engine.c $(CORE) -o $@

build/test-engine-sanitize: tests/engine.c $(CORE) $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(WARNINGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc tests/engine.c $(CORE) -o $@

test: nexir-runtime build/test-engine examples
	./nexir-runtime --validate build/crossing.nxg
	./nexir-runtime --validate build/bounce.nxg
	./build/test-engine

sanitize: build/test-engine-sanitize examples
	./build/test-engine-sanitize

run: nexir-gui examples
	./nexir-gui "$(GAME)"

smoke: all
	xvfb-run -a sh tests/x11-smoke.sh

clean:
	rm -rf build nexir-runtime nexir-gui
