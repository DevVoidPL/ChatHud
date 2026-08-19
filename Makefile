CC ?= cc
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc $(shell pkg-config --cflags x11 xfixes xext cairo pangocairo libcurl json-c libjpeg libwebp)
LDLIBS += $(shell pkg-config --libs x11 xfixes xext cairo pangocairo libcurl json-c libjpeg libwebp) -lpthread -lm

SRC = src/main.c src/config.c src/youtube.c src/ytdlp.c src/overlay.c src/emote.c src/image.c
OBJ = $(SRC:.c=.o)
BIN = chathud

.PHONY: all clean install uninstall

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) $(HOME)/.local/bin/$(BIN)
	install -Dm755 chathud-steam $(HOME)/.local/bin/chathud-steam
	mkdir -p $(HOME)/.config/chathud
	install -Dm644 config.example $(HOME)/.config/chathud/config.example
	@if [ ! -f $(HOME)/.config/chathud/config ]; then \
		install -Dm600 config.example $(HOME)/.config/chathud/config; \
	fi

uninstall:
	rm -f $(HOME)/.local/bin/$(BIN) $(HOME)/.local/bin/chathud-steam
