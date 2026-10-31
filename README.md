<p align="center">
  <img src="assets/chathud-banner.png" alt="ChatHUD — Your chat. In your game. YouTube Live chat for Linux, with Steam / Proton integration." width="100%" />
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-a78bfa?style=flat-square" alt="MIT license" /></a>
  <img src="https://img.shields.io/badge/platform-Linux-8b5cf6?style=flat-square" alt="Linux" />
  <img src="https://img.shields.io/badge/overlay-C-22d3ee?style=flat-square" alt="Overlay written in C" />
  <img src="https://img.shields.io/badge/settings-Qt%20%2F%20PySide6-c084fc?style=flat-square" alt="Qt / PySide6 settings app" />
  <img src="https://img.shields.io/badge/status-early%20alpha-f472b6?style=flat-square" alt="Early alpha" />
</p>

<p align="center">
  <strong>English</strong> · <a href="README_PL.md">Polski</a><br />
  <a href="#install">Install</a> · <a href="#set-up-your-chat">Set up your chat</a> · <a href="#steam-launch-options">Steam</a> · <a href="#troubleshooting">Troubleshooting</a> · <a href="CHANGELOG.md">Changelog</a>
</p>

# ChatHUD

**Keep YouTube Live chat visible while you play.**

ChatHUD adds a transparent, click-through chat panel over your Linux game. Paste a stream link, choose a monitor and save a game profile in the desktop app. The native C overlay handles the chat while you play; the Qt settings panel can stay closed.

Built for following a live chat on the screen you're already using, with a dedicated Steam/Proton launcher and optional MangoHud integration.

> **Current version: 0.6.1 · early alpha.** Installation currently uses a source build. Flatpak, AppImage and official distribution packages are planned; they are not available in this repository yet. Wayland sessions require XWayland for the overlay.

## What you can do

| Feature | What it gives you |
| --- | --- |
| **YouTube Live chat** | Use a public stream URL with the `yt-dlp` backend; no Google API key needed. |
| **Desktop settings app** | Adjust appearance, preview the layout and copy your Steam command. The interface is currently in Polish. |
| **Game profiles** | Keep separate stream URLs and appearance settings for each game. |
| **Monitor selection** | Pick the display for your chat. Corners and center positions are relative to that screen. |
| **Readable overlay** | Wrapped messages, channel header, font sizes, opacity, alignment and message order. |
| **Custom emoji** | YouTube custom emotes with local caching, plus Unicode text handled by Pango. |
| **Steam / Proton launcher** | Keep Steam's environment for the game while running the overlay with host libraries. |
| **Click-through input** | Keep using the mouse in your game. |

### In-game example

![ChatHUD showing YouTube Live chat in the upper-right corner of a Linux game](assets/chathud-ingame.png)

*Real in-game capture from an earlier build. The banner above is promotional artwork.*

## Install

You'll need Linux, a C compiler, Python 3.10+ for the desktop app, the native libraries below, and an X11/XWayland display for the overlay. `yt-dlp` retrieves the chat. MangoHud is optional.

### 1. Install dependencies

<details open>
<summary><strong>Arch Linux / EndeavourOS</strong></summary>

```bash
sudo pacman -S --needed \
  git base-devel pkgconf python pyside6 qt6-svg \
  libx11 libxrandr libxfixes libxext cairo pango \
  curl json-c libjpeg-turbo libwebp yt-dlp
```

</details>

<details>
<summary><strong>Debian 13 / Ubuntu 26.04</strong></summary>

```bash
sudo apt update
sudo apt install \
  git build-essential pkg-config python3 \
  python3-pyside6.qtwidgets python3-pyside6.qtsvg \
  libx11-dev libxrandr-dev libxfixes-dev libxext-dev \
  libcairo2-dev libpango1.0-dev libcurl4-openssl-dev \
  libjson-c-dev libjpeg-dev libwebp-dev yt-dlp
```

Ubuntu provides the PySide6 packages through its **Universe** repository. Package availability differs on older releases; use the fallback below if your release does not provide them.

</details>

<details>
<summary><strong>Older Debian / Ubuntu: Python environment for the desktop app</strong></summary>

Install the native build dependencies first:

```bash
sudo apt update
sudo apt install \
  git build-essential pkg-config python3 python3-venv \
  libx11-dev libxrandr-dev libxfixes-dev libxext-dev \
  libcairo2-dev libpango1.0-dev libcurl4-openssl-dev \
  libjson-c-dev libjpeg-dev libwebp-dev yt-dlp
```

Create a dedicated environment for the Qt settings app, using Python 3.10 or newer:

```bash
CHATHUD_VENV="${XDG_DATA_HOME:-$HOME/.local/share}/chathud/venv"
python3 -m venv "$CHATHUD_VENV"
"$CHATHUD_VENV/bin/python" -m pip install 'PySide6>=6.5,<7'
```

The `chathud-gui` launcher automatically uses this environment when it exists, including when launched from the application menu. Otherwise it uses the system Python. It does not require `sudo pip`.

</details>

### 2. Download and install ChatHUD

```bash
git clone https://github.com/DevVoidPL/ChatHud.git
cd ChatHud
./install.sh
```

Run `./install.sh` as your regular user. It builds the overlay, installs the launchers and desktop entry, and preserves an existing configuration. The dependency commands above are the steps that need `sudo`.

**Prefer a ZIP?** Use **Code → Download ZIP** on this repository, extract the archive, open a terminal in the extracted folder and run `bash install.sh` after installing the dependencies. The ZIP contains source code and still needs to be built.

### 3. Open the app

Launch **ChatHUD** from your Linux application menu, or run:

```bash
~/.local/bin/chathud-gui
```

## Set up your chat

1. Paste the URL of a specific YouTube Live stream into **Transmisja**.
2. Choose the target display in **Monitor**. Use **↻** to refresh the list after changing your display layout.
3. Set the position, width, font size, message count and background opacity. **Zaawansowane** contains alignment, message order, emotes and the optional YouTube API backend.
4. Use **Test na pulpicie** for sample messages without a game or internet connection. **Połącz z live’em** connects to your real stream. **■** stops the preview process.
5. Click **Kopiuj komendę**, then paste it into **Steam → your game → Properties → General → Launch Options**.

**For your next stream, change the URL and save the same profile. Your Steam command stays the same.** Saved settings apply the next time the overlay starts; restart the preview or game to load them.

Use **Dodaj profil** to select a locally detected Steam game or enter a name yourself. A new profile copies the current saved settings and then has its own configuration. Steam game names are read locally; the app leaves Steam's settings for you to edit.

## Steam Launch Options

The desktop app generates the command for your selected profile, including paths with spaces. If you already have custom launch options, merge them manually before pasting.

A command using the default profile looks like this:

```bash
$HOME/.local/bin/chathud-steam --config "$HOME/.config/chathud/config" -- %command%
```

With MangoHud installed:

```bash
$HOME/.local/bin/chathud-steam --config "$HOME/.config/chathud/config" -- mangohud %command%
```

You can also supply the URL directly. `--url` overrides the `stream_url` saved in the profile:

```bash
$HOME/.local/bin/chathud-steam --url "https://www.youtube.com/watch?v=VIDEO_ID" -- %command%
```

For a standalone overlay:

```bash
~/.local/bin/chathud --url "https://www.youtube.com/watch?v=VIDEO_ID" --no-game
```

## Configuration and files

| Default location | Contents |
| --- | --- |
| `~/.local/bin/chathud` | Native chat overlay |
| `~/.local/bin/chathud-steam` | Steam/Proton launcher |
| `~/.local/bin/chathud-gui` | Desktop settings launcher |
| `~/.local/share/chathud/` | Settings app, icon and optional Python environment |
| `~/.config/chathud/config` | Default profile |
| `~/.config/chathud/profiles/*.conf` | Per-game profiles |
| `~/.config/chathud/profiles.json` | Profile names and MangoHud selection |
| `~/.cache/chathud/steam-wrapper.log` | Steam launcher diagnostics |

`XDG_CONFIG_HOME`, `XDG_DATA_HOME` and `XDG_CACHE_HOME` are respected. The installer preserves the existing default config. Saving through the app preserves comments and unknown settings; config and profile metadata files are written atomically with private permissions.

See [config.example](config.example) for all options. A typical profile includes:

```ini
stream_url=https://www.youtube.com/watch?v=VIDEO_ID
provider=auto
monitor=primary
position=top-right
width=700
lines=6
font_size=18
background_opacity=0.62
```

Use `background_opacity=0` for text without a panel background. To choose a display by name, use the GUI or read the renderer's monitor list with `~/.local/bin/chathud --list-monitors`. An unavailable saved display falls back to the primary display, or the first active display if none is marked primary.

<details>
<summary><strong>Optional official YouTube Data API backend</strong></summary>

Set **Źródło czatu → YouTube API** and enter your own API key, or edit the profile:

```ini
provider=api
youtube_api_key=YOUR_API_KEY
```

`provider=auto` uses `yt-dlp` when available and otherwise tries the official API backend. A valid API key is required for that fallback.

</details>

## Troubleshooting

| Symptom | What to check |
| --- | --- |
| Build reports a missing package | Install the development dependencies above; the compiler needs headers as well as runtime libraries. |
| The desktop app reports missing PySide6 | Install the distro Qt bindings or create the dedicated environment described above. |
| Chat is on the wrong screen | Choose **Monitor**, then save the profile and restart the overlay. |
| No overlay under Wayland | Check that XWayland is running and `DISPLAY` is set. Visibility above native Wayland games depends on the compositor. |
| No messages from YouTube | Check the stream link, whether live chat is enabled, and your `yt-dlp` version. Use **Diagnostyka** in the app. |
| Steam integration fails | Read `~/.cache/chathud/steam-wrapper.log`; include it when reporting a problem. |
| An older taskbar icon remains pinned | Close the app, reopen **ChatHUD** from the application menu and pin the current launcher. |

When reporting an issue, include your distro, X11/Wayland session, ChatHUD version, relevant terminal output and the Steam wrapper log. Redact API keys and other private information before sharing logs.

## Update or uninstall

For a Git checkout with no local modifications:

```bash
cd ChatHud
git pull --ff-only
./install.sh
```

Close the settings app and overlay before updating, then reopen them. Saved profiles are retained. If you downloaded a ZIP, download and extract the updated source and run its installer again.

Uninstall from the source directory:

```bash
make uninstall
```

This removes installed app files and the optional app-local Python environment. Your settings and chat cache remain in the locations listed above.

## Current scope and roadmap

ChatHUD currently supports **YouTube Live on Linux through X11/XWayland**. It is an external overlay; visibility over particular games depends on the window system and compositor. The YouTube backend also depends on changes to YouTube and keeping `yt-dlp` current.

Planned directions include native Wayland support, Twitch chat and emotes, a show/hide hotkey, message fading, richer badges and easier distribution through packages or Flatpak. These are roadmap items, not implemented features.

## For contributors

The renderer is written in C using X11, XRandR, XFixes/XShape, Cairo and Pango. Chat retrieval uses `yt-dlp` or libcurl/json-c with the YouTube API. The optional editor uses Python and PySide6.

The repository includes `make test` for the regression checks. GUI checks require PySide6; native display checks need an accessible X11/XWayland display. See [CHANGELOG.md](CHANGELOG.md) for version history.

Released under the [MIT License](LICENSE).
