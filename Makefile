
CC = gcc
CFLAGS = -std=c99 -fPIC -Wall -Wextra -pedantic \
		 -Wfloat-equal -Wshadow -Wno-unused-parameter \
		 -Wswitch-enum -Wcast-qual -Wnull-dereference -Wunused-result # -Waggregate-return
INCLUDES = -Isrc/ -I/usr/include/SDL2 -Ilib/nanovg/src -Ilib/stb
LIBS = -lm -lGL -lSDL2 -lSDL2_mixer -lSDL2_net # -lSDL2_ttf

SRC = main.c src/engine.c \
	  src/scenes/scene.c src/scenes/game.c src/scenes/intro.c src/scenes/menu.c \
	  src/game/terrain.c \
	  src/util/easing.c \
	  lib/nanovg/src/nanovg.c lib/stb/stb_ds.c
OBJ = $(SRC:.c=.o)
TARGET = soil_soldiers

# when compiling with emscripten, add some specific flags
ifeq ($(CC), emcc)
	# TODO: dont add everything to cflags, some flags should be used only during linking
	CFLAGS += -sWASM=0 \
			  -sUSE_SDL=2 -sUSE_SDL_NET=2 -sUSE_SDL_MIXER=2 -sFULL_ES2=1 \
			  --preload-file res \
			  --shell-file src/web/shell.html # -sUSE_SDL_TTF=2
	TARGET = soil_soldiers.html
endif

.PHONY: all

all: $(TARGET)

# debug-specific
debug: CFLAGS += -DDEBUG -ggdb -O0
debug: LIBS += -ldl
debug: $(TARGET)
# release-specific
release: CFLAGS += -O2
release: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

