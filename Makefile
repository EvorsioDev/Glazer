NAME=glazer

CC=gcc
SOURCES=$(shell find src/ -type f -name '*.c')
OBJECTS=$(subst src/,build/,$(SOURCES:.c=.o))
HEADERS=$(shell find src/ -type f -name '*.h')
LDFLAGS=-lm -nostdlib++

CFLAGS=-Wall \
			 $(shell pkg-config --cflags sqlite3) \
				-std=gnu11 \
				-Wextra \
				-ggdb  \
				-pedantic \
				-Isrc \
				-O2 \
				-Wconversion \
				-Wshadow \
				-Wlogical-op \
				-Wshift-overflow=2 \
				-Wduplicated-cond \
				-fstack-protector \
				-Wstrict-prototypes\

RELEASE_CFLAGS:=$(CFLAGS) -DRELEASE_MODE

build-test: $(OBJECTS) | test
	$(CC) $(CFLAGS) $^ -o build/$(NAME)  $(LDFLAGS) -fsanitize=address,undefined

build-release: CFLAGS=$(RELEASE_CFLAGS)
build-release: $(OBJECTS) | test
	$(CC) $(CFLAGS) $^ -o build/$(NAME)  $(LDFLAGS)


# build/dimagram: $(OBJECTS) | test
# 	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build-folder:
	mkdir -p build

build/%.o: src/%.c $(HEADERS) | build-folder
	mkdir -p $(shell dirname "$@")
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean test build-release build-test

clean:
	rm -rfv build
	$(MAKE) -C test clean

test:
	$(MAKE) -C test

