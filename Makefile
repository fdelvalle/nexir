CC ?= cc
CFLAGS ?= -O2 -g
WARNINGS = -std=c11 -Wall -Wextra -Wpedantic
X11_CFLAGS ?= $(shell pkg-config --cflags x11)
X11_LIBS ?= $(shell pkg-config --libs x11)
CORE = src/parser.c src/engine.c
HEADERS = src/nxg.h src/engine.h
LEGACY_GAMES = build/legacy/river.nxg build/legacy/river_fast.nxg build/legacy/river_training.nxg
LEGACY_APPS = build/legacy/wallet.nx build/legacy/calculator.nx build/legacy/toggle.nx
GAME ?= build/crossing.nxg

.PHONY: all examples legacy-fixtures test sanitize run smoke clean
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -UNDEBUG -Isrc $(LDFLAGS) tests/engine.c $(CORE) -o $@

build/test-engine-sanitize: tests/engine.c $(CORE) $(HEADERS) Makefile
	mkdir -p build
	$(CC) $(CPPFLAGS) $(WARNINGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -Isrc tests/engine.c $(CORE) -o $@

legacy-fixtures: $(LEGACY_GAMES) $(LEGACY_APPS)

$(LEGACY_GAMES) &: make_game.py
	mkdir -p build/legacy
	cd build/legacy && python3 ../../make_game.py

$(LEGACY_APPS) &: build.py
	mkdir -p build/legacy
	cp build.py build/legacy/
	python3 build/legacy/build.py

build/legacy/test_game_vm: test_game_vm.c game_vm.h Makefile
	mkdir -p build/legacy
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNINGS) -UNDEBUG $(LDFLAGS) test_game_vm.c -lm -o $@

test: nexir-runtime build/test-engine examples legacy-fixtures build/legacy/test_game_vm
	./nexir-runtime --validate build/crossing.nxg
	./nexir-runtime --validate build/bounce.nxg
	./build/test-engine
	cd build/legacy && ./test_game_vm

sanitize: build/test-engine-sanitize examples
	./build/test-engine-sanitize

run: nexir-gui examples
	./nexir-gui "$(GAME)"

smoke: all
	xvfb-run -a sh tests/x11-smoke.sh

clean:
	rm -rf build nexir-runtime nexir-gui
