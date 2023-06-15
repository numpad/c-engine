
CC = gcc
CFLAGS = -std=c99 -Wall -pedantic
INCLUDES = -I/usr/include/SDL2
LIBS = -lSDL2 -lGL

SRC = main.c src/*.c
OBJ = $(SRC:.c=.o)
TARGET = soil_soldiers

# when compiling with emscripten, add some specific flags
ifeq ($(CC), emcc)
	CFLAGS += -sWASM=0 -sUSE_SDL=2 -sFULL_ES2=1 --shell-file shell.html
	TARGET = soil_soldiers.html
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

