# Changelog

All notable changes to ChatHUD will be documented here.

## [0.7.0] - 2026-10-31

### Added
- English and Polish UI with a persistent language selector; switching keeps the current input and unsaved settings.
- Twitch channel URLs with a read-only anonymous IRC backend over TLS, reconnects, keepalive handling and moderation deletions.
- English and Polish built-in overlay status messages.

### Changed
- Stream URLs select the platform automatically; the YouTube backend setting only applies to YouTube.
- Twitch works independently of yt-dlp and YouTube API keys.
- Shorter English and Polish installation and usage documentation.

### Limitations
- Twitch emote images, BTTV, FFZ and 7TV are not implemented; emote names remain text.
- Anonymous IRC access depends on Twitch continuing to allow guest readers.

## [0.6.1] - 2026-10-31

### Added
- Branded GitHub banner and social-preview image, with complete English and Polish installation guides.
- Automatic desktop-launcher support for an app-local PySide6 virtual environment on distributions without native PySide6 packages.

### Fixed
- Overlay positions and panel bounds now use the selected monitor instead of the combined desktop.
- Monitor selection in the GUI uses the renderer's XRandR names and persists per profile.
- Default to the primary monitor; fall back to it when a selected display is disconnected.
- React to monitor layout changes without moving the chat onto a neighboring display.
- Taskbar identity on Wayland/X11 now matches a canonical desktop entry, with an explicit PNG icon and application-wide window icon.
- Existing pinned launchers retain a compatible hidden desktop entry.

## [0.6.0] - 2026-10-31

### Added
- Native PySide6 desktop settings app, Polish UI, app icon and menu entry.
- Custom pixel-art ChatHUD icon for Linux, app header and GitHub README branding.
- Independent per-game profiles and local Steam game-name discovery.
- In-app appearance preview plus real desktop demo/live test with stop controls.
- Copy Steam Launch Options, including optional MangoHud.
- Persistent `stream_url` config key; explicit `--url` takes precedence.
- Dependency/session diagnostics, atomic private profile writes, regression checks.

### Fixed
- Bounded and interruptible yt-dlp channel lookup and downloader shutdown.
- Steam wrapper cleans up its overlay when executing the game fails.
- Installer no longer prints a hard-coded developer home directory.
- Installation respects XDG config/data directories and preserves existing config.
- Header dependency tracking avoids stale C objects after config changes.

## [0.5.1] - 2026-08-19

### Fixed
- Fixed a stack overflow / segmentation fault in the overlay renderer after rich emote support was introduced.
- Large render buffers are now allocated on the heap instead of the stack.
- Markdown-formatted YouTube URLs are sanitized before being passed to the backend.

## [0.5.0] - 2026-08-19

### Added
- Wrapped multi-line chat messages instead of truncating them with ellipsis.
- YouTube channel name in the overlay header.
- Configurable font size, header size, width, message count and background opacity.
- Nine screen positions and text alignment settings.
- YouTube custom emoji rendering with local caching.

## [0.4.0] - 2026-08-19

### Added
- Separate `chathud-steam` launcher for Steam/Proton compatibility.
- Steam launcher logging under `~/.cache/chathud/steam-wrapper.log`.

## [0.1.0] - 2026-08-19

### Added
- Initial native X11/XWayland overlay.
- YouTube Live chat through yt-dlp.
- Basic Steam launch-options integration.
