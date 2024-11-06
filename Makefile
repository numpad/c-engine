
CC = clang

# Sadly, flecs requires `gnu99`.
CFLAGS = -std=gnu99 -fPIC -Wall -Wextra -pedantic \
		 -D_GNU_SOURCE \
		 -Wfloat-equal -Wshadow -Wno-unused-parameter -Wl,--export-dynamic \
		 -Werror=switch-enum -Wcast-qual -Wnull-dereference -Wunused-result \
		 -DFLECS_CUSTOM_BUILD -DFLECS_SYSTEM -DFLECS_MODULE -DFLECS_PIPELINE -DFLECS_TIMER -DFLECS_HTTP -DFLECS_SNAPSHOT -DFLECS_PARSER -DFLECS_APP -DFLECS_OS_API_IMPL \

# TODO: We may want to provide SDL (& modules), freetype and harfbuzz?
#       For now this is fine, as emscripten handles this for us as well.
#       Also, harfbuzz needs to be compiled with C++.
INCLUDES = -I src/ \
		   -isystem /usr/include/SDL2 \
		   -isystem /usr/include/freetype2 \
		   -isystem /usr/include/harfbuzz \
		   -isystem lib/cgltf \
		   -isystem lib/nanovg/src \
		   -isystem lib/stb \
		   -isystem lib/cglm/include \
		   -isystem lib/flecs \
		   -isystem lib/cJSON \
		   -isystem lib/box2d/include/
LIBS = -lm -lGL -lSDL2 -lSDL2_mixer -lSDL2_net -lfreetype -lharfbuzz

BIN = bin/native/
TARGET = soil_soldiers

# when compiling with emscripten, add some specific flags
ifeq ($(CC), emcc)
	# TODO: Let's not add everything to CFLAGS, some should only be used when linking.
	# TODO: Does `-simd128 -msse2` reduce the devices we can target?
	# TODO: Check these flags: `-sASYNCIFY -sWEBSOCKET_SUBPROTOCOL="binary"`
	# TODO: Find out what implications or improvements this has `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2`.
	# TODO: Figure out if we want to support non-secure websockets as well? Maybe for local browser development? `-sWEBSOCKET_URL="wss://"`
	CFLAGS += -sWASM=1 \
			  -sUSE_SDL=2 -sUSE_SDL_NET=2 -sUSE_SDL_MIXER=2 -sUSE_SDL_IMAGE=0 -sUSE_SDL_TTF=0 -sUSE_FREETYPE=1 -sUSE_HARFBUZZ=1 \
			  -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 \
			  -sALLOW_MEMORY_GROWTH=1 \
			  -sWEBSOCKET_URL="wss://" \
			  -sINITIAL_MEMORY=128MB -sTOTAL_STACK=64MB \
			  -sEXPORTED_RUNTIME_METHODS=cwrap \
			  -sEXPORTED_FUNCTIONS=_main,_on_siggoback \
			  -sMODULARIZE=1 -sEXPORT_NAME="MyApp" \
			  --preload-file res \
			  --shell-file src/web/shell_start_on_click.html \
			  -msimd128 -msse2
	TARGET = soil_soldiers.html
	BIN = bin/emcc/
endif

SRC = main.c src/engine.c src/platform.c \
	  $(wildcard src/gui/*.c) \
	  $(wildcard src/game/*.c) \
	  $(wildcard src/util/*.c) \
	  $(wildcard src/gl/*.c) \
	  src/net/message.c \
	  src/server/errors.c \
	  $(wildcard lib/stb/*.c) \
	  $(wildcard lib/cglm/src/*.c) \
	  $(wildcard lib/box2d/src/*.c) \
	  lib/nanovg/src/nanovg.c \
	  lib/cJSON/cJSON.c \
	  lib/flecs/flecs.c \
	  lib/cgltf/cgltf.c \
	  $(wildcard src/scenes/*.c)
OBJ = $(addprefix $(BIN),$(SRC:.c=.o))

.PHONY: all clean scenes server

all: $(TARGET)

server:
	$(MAKE) -f src/server/Makefile


# TODO: Let's do a cleanup of the following mess...

# debug-specific
debug: CFLAGS += -DDEBUG -ggdb -O0
debug: LIBS += -ldl
debug: $(TARGET)
# release-specific
release: CFLAGS += -O2
release: $(TARGET)

$(BIN)lib/%.o: CFLAGS += -w

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(BIN)%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

scenes: CFLAGS += -DDEBUG -ggdb -O0
scenes: LIBS += -ldl
scenes:
	$(CC) $(CFLAGS) -shared -o hotreload.so $(INCLUDES) $(LIBS) \
		src/scenes/battle.c src/gui/console.c

clean:
	rm -rf $(BIN) $(TARGET) "$(TARGET).data" "$(TARGET).html" "$(TARGET).js" "$(TARGET).wasm"

