
CC = clang

# Sadly, flecs requires `gnu99`.
CFLAGS = -std=gnu99 -fPIC -Wall -Wextra -pedantic                            \
		 -Wl,--export-dynamic                                                \
		 -D_GNU_SOURCE                                                       \
		 -Wfloat-equal -Wshadow -Wno-unused-parameter                        \
		 -Werror=switch-enum -Wcast-qual -Wnull-dereference -Wunused-result  \
		 -DFLECS_CUSTOM_BUILD -DFLECS_SYSTEM -DFLECS_MODULE -DFLECS_PIPELINE \
		 -DFLECS_TIMER -DFLECS_HTTP -DFLECS_SNAPSHOT -DFLECS_PARSER          \
		 -DFLECS_APP -DFLECS_OS_API_IMPL -DFLECS_JSON

# Sanitizer runtime is not compatible with RTLD_DEEPBIND, so hot reload does not work.
CFLAGS_EXTRA_SAFE = -Warray-bounds -Warray-bounds-pointer-arithmetic        \
					-Wassign-enum -Wbad-function-cast -Wcast-align          \
					-Wcast-qual -Wdouble-promotion                          \
					-Wformat=2 -Wformat-security                            \
					-Wframe-larger-than=2048 -Wlogical-op                   \
					-Wnull-dereference -Wpointer-arith                      \
					-Wstrict-overflow=5 -Wwrite-strings                     \
					-fstack-protector-strong -fno-strict-aliasing           \
					-fno-delete-null-pointer-checks -fno-omit-frame-pointer \
					-fsanitize=address -fsanitize=undefined -fsanitize=leak \
					-D_FORTIFY_SOURCE=2

# TODO: We may want to provide SDL (& modules), freetype and harfbuzz?
#       For now this is fine, as emscripten handles this for us as well.
#       Also, harfbuzz needs to be compiled with C++.
INCLUDES = -I src/                         \
		   -isystem /usr/include/SDL2      \
		   -isystem /usr/include/freetype2 \
		   -isystem /usr/include/harfbuzz  \
		   -isystem lib/cgltf              \
		   -isystem lib/nanovg/src         \
		   -isystem lib/stb                \
		   -isystem lib/cglm/include       \
		   -isystem lib/flecs              \
		   -isystem lib/cJSON              \
		   -isystem lib/box2d/include/
LIBS = -lm -lGL -lSDL2 -lSDL2_mixer -lSDL2_net -lfreetype -lharfbuzz

BIN = bin/native/
TARGET = cengine

# when compiling with emscripten, add some specific flags
ifeq ($(CC), emcc)
	TARGET = cengine.html
	BIN = bin/emcc/
	SHELL_FILE = src/web/shell.html

	# TODO: Let's not add everything to CFLAGS, some should only be used when linking.
	# TODO: Does `-simd128 -msse2` reduce the devices we can target?
	# TODO: Check these flags: `-sASYNCIFY -sWEBSOCKET_SUBPROTOCOL="binary"`
	# TODO: Find out what implications or improvements this has `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`.
	# TODO: Figure out if we want to support non-secure websockets as well? Maybe for local browser development? `-sWEBSOCKET_URL="wss://"`
	CFLAGS += -sWASM=1                                                        \
			  -sUSE_SDL=2 -sUSE_SDL_NET=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_IMAGE=0 \
			  -sUSE_SDL_TTF=0 -sUSE_FREETYPE=1 -sUSE_HARFBUZZ=1               \
			  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2                     \
			  -sALLOW_MEMORY_GROWTH=1                                         \
			  -sWEBSOCKET_URL="wss://"                                        \
			  -sINITIAL_MEMORY=128MB -sTOTAL_STACK=64MB                       \
			  -sEXPORTED_RUNTIME_METHODS=cwrap                                \
			  -sEXPORTED_FUNCTIONS=_main,_on_siggoback                        \
			  -sMODULARIZE=1 -sEXPORT_NAME="MyApp"                            \
			  --preload-file res                                              \
			  --shell-file $(SHELL_FILE)                                      \
			  -msimd128 -msse2
endif

SRC = main.c                        \
	  src/engine.c src/platform.c   \
	  $(wildcard src/gui/*.c)       \
	  $(wildcard src/game/*.c)      \
	  $(wildcard src/util/*.c)      \
	  $(wildcard src/gl/*.c)        \
	  src/net/message.c             \
	  src/server/errors.c           \
	  $(wildcard lib/stb/*.c)       \
	  $(wildcard lib/cglm/src/*.c)  \
	  $(wildcard lib/box2d/src/*.c) \
	  lib/nanovg/src/nanovg.c       \
	  lib/cJSON/cJSON.c             \
	  lib/flecs/flecs.c             \
	  lib/cgltf/cgltf.c             \
	  $(wildcard src/scenes/*.c)

OBJ = $(addprefix $(BIN),$(SRC:.c=.o))


.PHONY: all clean scenes server

all: release

server:
	$(MAKE) -f src/server/Makefile

clean:
	rm -rf $(BIN) $(TARGET) "$(TARGET).data" "$(TARGET).html" "$(TARGET).js" "$(TARGET).wasm"

# TODO: Let's do a cleanup of the following mess...

# Main Target

extra_debug: CFLAGS += $(CFLAGS_EXTRA_SAFE)
extra_debug: debug

# Debug Flags
debug: CFLAGS += -DDEBUG -ggdb -O0
debug: LIBS += -ldl
debug: $(TARGET)

# Release Flags
release: CFLAGS += -O2
release: $(TARGET)

# Disable warnings in library code.
$(BIN)lib/%.o: CFLAGS += -w

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(BIN)%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


# Tests
TEST_SRC = $(wildcard src/tests/framework/*.c) $(wildcard src/tests/test_*.c)
TEST_OBJ = $(addprefix $(BIN),$(TEST_SRC:.c=.o))
TEST_EXEC = run_tests

SRC_NO_MAIN = $(filter-out main.c, $(SRC))
OBJ_NO_MAIN = $(addprefix $(BIN),$(SRC_NO_MAIN:.c=.o))

test: $(OBJ_NO_MAIN) $(TEST_OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST_EXEC) $(OBJ_NO_MAIN) $(TEST_OBJ) $(LIBS)
	./$(TEST_EXEC)

$(BIN)tests/%.o: tests/%.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@


# Hot-reload
scenes: CFLAGS += -DDEBUG -ggdb -O0
scenes: LIBS += -ldl
scenes:
	$(CC) $(CFLAGS) -shared -o hotreload.so $(INCLUDES) $(LIBS) \
		src/scenes/battle.c src/game/hexmap.c

