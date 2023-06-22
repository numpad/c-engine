
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -Wfloat-equal -Wshadow -Waggregate-return -Wno-unused-parameter -Wswitch-enum -Wcast-qual -Wnull-dereference -Wunused-result
INCLUDES = -Isrc/ -I/usr/include/SDL2
LIBS = -lm -lSDL2 -lSDL2_gfx -lSDL2_mixer -lSDL2_net -lGL # -lSDL2_ttf

SRC = main.c src/engine.c src/scenes/game.c src/scenes/scene.c src/scenes/intro.c src/scenes/menu.c
OBJ = $(SRC:.c=.o)
TARGET = soil_soldiers

# when compiling with emscripten, add some specific flags
ifeq ($(CC), emcc)
	# TODO: dont add everything to cflags, some flags should be used only during linking
	CFLAGS += -sWASM=0 -sUSE_SDL=2 -sUSE_SDL_NET=2 -sUSE_SDL_GFX=2 -sUSE_SDL_TTF=2 -sUSE_SDL_MIXER=2 -sFULL_ES2=1 --preload-file res --shell-file src/web/shell.html
	TARGET = soil_soldiers.html
endif

.PHONY: all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

