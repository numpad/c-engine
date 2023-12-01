
CC = gcc
CFLAGS = -std=gnu99 -fPIC -Wall -Wextra -pedantic \
		 -Wfloat-equal -Wshadow -Wno-unused-parameter -Wl,--export-dynamic \
		 -Wswitch-enum -Wcast-qual -Wnull-dereference -Wunused-result
INCLUDES = -Isrc/ -I/usr/include/SDL2 -Ilib/stb -Ilib/cglm/include -Ilib/cJSON
LIBS = -lm -lSDL2 -lSDL2_net -lncurses -lwebsockets -lcap

BIN = bin/server/
TARGET = server

SRC = src/server/server_main.c lib/stb/stb_ds.c $(wildcard lib/cglm/src/*.c) lib/cJSON/cJSON.c
OBJ = $(addprefix $(BIN),$(SRC:.c=.o))

.PHONY: all clean

all: $(TARGET)

# debug-specific
debug: CFLAGS += -DDEBUG -ggdb -O0
debug: $(TARGET)
# release-specific
release: CFLAGS += -O2
release: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LIBS)

$(BIN)%.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BIN) $(TARGET)

