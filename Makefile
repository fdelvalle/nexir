CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -g
CORE_FLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -Icore/include
EXAMPLE_FLAGS = -Icore/examples
CORE_SOURCE = core/src/common.c core/src/binary.c core/src/verify.c core/src/vm.c core/src/inspect.c
CORE_OBJECT = $(CORE_SOURCE:core/src/%.c=build/core/%.o)
CORE_HEADERS = core/src/internal.h core/include/nexir.h
EXAMPLES = core/examples/programs.c
EXAMPLE_HEADERS = core/examples/programs.h
LIBRARY = build/core/libnexir.a

.PHONY: all core test sanitize demo
all: core
core: $(LIBRARY)
demo: build/core/nexir
	./build/core/nexir

$(LIBRARY): $(CORE_OBJECT)
	$(AR) rcs $@ $^

build/core/%.o: core/src/%.c $(CORE_HEADERS) Makefile
	mkdir -p build/core
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_FLAGS) -c $< -o $@

build/core/nexir: core/examples/demo.c $(EXAMPLES) $(EXAMPLE_HEADERS) $(LIBRARY) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_FLAGS) $(EXAMPLE_FLAGS) $(LDFLAGS) core/examples/demo.c $(EXAMPLES) $(LIBRARY) -o $@

build/core/test: core/tests/conformance.c $(EXAMPLES) $(EXAMPLE_HEADERS) $(LIBRARY) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CORE_FLAGS) $(EXAMPLE_FLAGS) -UNDEBUG $(LDFLAGS) core/tests/conformance.c $(EXAMPLES) $(LIBRARY) -o $@

build/core/test-sanitize: $(CORE_SOURCE) $(CORE_HEADERS) $(EXAMPLES) $(EXAMPLE_HEADERS) core/tests/conformance.c Makefile
	mkdir -p build/core
	$(CC) $(CPPFLAGS) $(CORE_FLAGS) $(EXAMPLE_FLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -UNDEBUG $(LDFLAGS) $(CORE_SOURCE) $(EXAMPLES) core/tests/conformance.c -o $@

test: build/core/test demo
	./build/core/test
	./build/core/nexir --inspect build/core/panel.nxir

sanitize: build/core/test-sanitize
	./build/core/test-sanitize
