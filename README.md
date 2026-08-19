# ChatHUD

**A lightweight native Linux overlay that puts YouTube Live chat directly on top of your game.**

ChatHUD is aimed at people who stream or play while following a live chat but do not have a second monitor and do not want to keep checking a phone. Give it a YouTube Live URL, launch the game, and the latest chat messages stay visible as a click-through HUD.

![ChatHUD running over a game](assets/chathud-ingame.png)

> **Project status:** early alpha. It works, but expect rough edges and breaking changes while the project is still young.

## Why ChatHUD?

Browser overlays are flexible, but they are often overkill when all you want is a few chat messages on the same screen as your game. ChatHUD is written in **C** and uses native Linux libraries instead of Electron or an embedded browser.

Current goals:

- low CPU and memory overhead,
- readable chat while playing on a single monitor,
- Steam/Proton-friendly launch options,
- click-through overlay that does not steal mouse input,
- customizable position and appearance,
- support for rich chat content such as YouTube custom emoji.

## Features

- YouTube Live chat from a normal stream URL
- no Google API key required for the default `yt-dlp` backend
- native X11/XWayland transparent overlay
- Steam/Proton launcher
- click-through window
- automatic channel name in the header
- wrapped multi-line messages
- YouTube custom emoji rendering and caching
- configurable font sizes, width, opacity and message count
- nine screen positions
- left / center / right text alignment
- oldest-first or newest-first message ordering
- optional official YouTube Data API backend
- works alongside MangoHud

## Requirements

ChatHUD currently targets Linux with X11 or XWayland.

### Arch Linux

```bash
sudo pacman -S --needed \
  base-devel pkgconf \
  libx11 libxfixes libxext \
  cairo pango curl json-c \
  libjpeg-turbo libwebp yt-dlp
```

### Ubuntu / Debian

```bash
sudo apt install \
  build-essential pkg-config \
  libx11-dev libxfixes-dev libxext-dev \
  libcairo2-dev libpango1.0-dev \
  libcurl4-openssl-dev libjson-c-dev \
  libjpeg-dev libwebp-dev yt-dlp
```

## Build and install

```bash
git clone https://github.com/DevVoidPL/ChatHUD.git
cd ChatHUD
make -j"$(nproc)"
make install
```

Or use the helper:

```bash
./install.sh
```

This installs:

```text
~/.local/bin/chathud
~/.local/bin/chathud-steam
~/.config/chathud/config
```

The installer preserves an existing user config.

## Quick test

Test the overlay without a game or network connection:

```bash
chathud --demo --no-game
```

Then try a real public YouTube Live stream:

```bash
chathud --url "https://www.youtube.com/watch?v=VIDEO_ID" --no-game
```

## Steam Launch Options

For Steam games, use the dedicated launcher rather than running `chathud` directly as the Steam wrapper:

```bash
$HOME/.local/bin/chathud-steam --url "https://www.youtube.com/watch?v=VIDEO_ID" -- %command%
```

The launcher starts ChatHUD with a host-library environment while leaving the original Steam/Proton environment intact for the game.

### With MangoHud

```bash
$HOME/.local/bin/chathud-steam --url "https://www.youtube.com/watch?v=VIDEO_ID" -- mangohud %command%
```

If Steam launch integration fails, check:

```text
~/.cache/chathud/steam-wrapper.log
```

## Configuration

The main configuration file is:

```text
~/.config/chathud/config
```

Example:

```ini
provider=auto
youtube_api_key=

lines=6
width=700

font_size=18
header_font_size=16
show_header=1

# 0.0 = fully transparent, 1.0 = opaque
background_opacity=0.62

margin=24

# top-left, top-center, top-right
# center-left, center, center-right
# bottom-left, bottom-center, bottom-right
position=top-right

# left, center, right
text_align=left

# oldest-first or newest-first
message_order=oldest-first

emotes=1
emote_size=24

refresh_ms=100
```

For a text-only HUD without the dark panel:

```ini
background_opacity=0
```

## How it works

```text
Steam
  |
  v
chathud-steam -----------------------> game / Proton
  |
  +--> ChatHUD worker
         |
         +--> yt-dlp / YouTube API
         |       |
         |       v
         |   live chat events
         |       |
         v       v
      ChatMessage buffer
             |
             v
       X11/XWayland overlay
```

The project is written in **C**.

Main components:

- **X11 + XFixes/XShape** — transparent click-through window
- **Cairo** — drawing
- **Pango** — UTF-8 text layout and wrapping
- **libcurl** — HTTP requests
- **json-c** — JSON parsing
- **libjpeg / libwebp** — image decoding for chat emoji
- **pthread** — chat backend work outside the render loop
- **yt-dlp** — default YouTube Live chat provider

ChatHUD is currently an external overlay, not a Vulkan layer like MangoHud. That is intentional for the early versions because it keeps the code smaller and easier to debug.

## YouTube providers

The default setting is:

```ini
provider=auto
```

When `yt-dlp` is available, ChatHUD uses it to retrieve YouTube Live chat. This lets the user provide only the stream URL without configuring a Google API key.

You can force it with:

```ini
provider=yt-dlp
```

There is also an official YouTube Data API fallback:

```ini
provider=api
youtube_api_key=YOUR_API_KEY
```

## Roadmap

Planned / considered features:

- Twitch chat backend
- Twitch emotes
- message fade-out and timeout
- configurable chat growth direction (`up` / `down`)
- text outline / shadow for fully transparent backgrounds
- runtime show/hide hotkey
- better badge and membership rendering
- per-author colors
- more robust Wayland support
- packaging for Arch / AUR and other distributions

## Known limitations

- Wayland support currently relies on XWayland.
- This is an early alpha and the configuration format may change.
- YouTube can change its internal live-chat format, so the `yt-dlp` backend depends on keeping yt-dlp reasonably current.
- Twitch is not implemented yet.

## Contributing

Bug reports, ideas and pull requests are welcome. For crashes, please include the terminal output and, when applicable, the Steam wrapper log from:

```text
~/.cache/chathud/steam-wrapper.log
```

## License

ChatHUD is released under the [MIT License](LICENSE).
