CC ?= cc
CFLAGS ?= -O2 -pipe -Wall -Wextra -Wpedantic
PREFIX ?= $(HOME)/.local
CONFIG_HOME ?= $(if $(XDG_CONFIG_HOME),$(XDG_CONFIG_HOME),$(HOME)/.config)
DATA_HOME ?= $(if $(XDG_DATA_HOME),$(XDG_DATA_HOME),$(HOME)/.local/share)
CPPFLAGS += -Isrc $(shell pkg-config --cflags x11 xrandr xfixes xext cairo pangocairo libcurl json-c libjpeg libwebp)
LDLIBS += $(shell pkg-config --libs x11 xrandr xfixes xext cairo pangocairo libcurl json-c libjpeg libwebp) -lpthread -lm

SRC = src/main.c src/monitor.c src/config.c src/youtube.c src/ytdlp.c src/overlay.c src/emote.c src/image.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)
BIN = chathud

.PHONY: all clean install uninstall test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEP)

clean:
	rm -f $(OBJ) $(DEP) $(BIN)

install: $(BIN)
	install -Dm755 $(BIN) "$(PREFIX)/bin/$(BIN)"
	install -Dm755 chathud-steam "$(PREFIX)/bin/chathud-steam"
	install -Dm755 chathud-gui "$(PREFIX)/bin/chathud-gui"
	install -Dm644 gui/app.py "$(DATA_HOME)/chathud/gui/app.py"
	install -Dm644 gui/core.py "$(DATA_HOME)/chathud/gui/core.py"
	install -Dm644 assets/chathud.png "$(DATA_HOME)/chathud/assets/chathud.png"
	install -Dm644 assets/check.svg "$(DATA_HOME)/chathud/assets/check.svg"
	python3 gui/install_desktop.py "$(PREFIX)" "$(DATA_HOME)" chathud.desktop
	mkdir -p "$(CONFIG_HOME)/chathud"
	install -Dm644 config.example "$(CONFIG_HOME)/chathud/config.example"
	@if [ ! -f "$(CONFIG_HOME)/chathud/config" ]; then \
		install -Dm600 config.example "$(CONFIG_HOME)/chathud/config"; \
	fi

uninstall:
	rm -f "$(PREFIX)/bin/$(BIN)" "$(PREFIX)/bin/chathud-steam" "$(PREFIX)/bin/chathud-gui"
	rm -f "$(DATA_HOME)/applications/chathud.desktop" "$(DATA_HOME)/applications/io.github.DevVoidPL.ChatHUD.desktop" "$(DATA_HOME)/icons/hicolor/scalable/apps/chathud.svg"
	rm -rf "$(DATA_HOME)/chathud"

test: $(BIN)
	python3 -m unittest discover -s tests -v
