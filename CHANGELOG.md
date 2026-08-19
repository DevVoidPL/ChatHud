# Changelog

All notable changes to ChatHUD will be documented here.

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
