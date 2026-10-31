#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
printf '==> Buduję ChatHUD 0.6.1\n'
make -j"$(nproc)"
printf '==> Instaluję overlay, launcher Steam i panel graficzny\n'
make install
venv_python="${XDG_DATA_HOME:-$HOME/.local/share}/chathud/venv/bin/python"
python_bin="${CHATHUD_PYTHON:-python3}"
if [[ -z "${CHATHUD_PYTHON:-}" && -x "$venv_python" ]]; then
    python_bin="$venv_python"
fi
if ! "$python_bin" -c 'import PySide6.QtWidgets, PySide6.QtSvg' 2>/dev/null; then
    printf '\nPanel wymaga PySide6.\nArch: sudo pacman -S pyside6 qt6-svg\nDebian 13/Ubuntu 26.04: sudo apt install python3-pyside6.qtwidgets python3-pyside6.qtsvg\n'
    printf 'Starsze wydania: instrukcja osobnego środowiska Pythona jest w README_PL.md.\n'
fi
if ! command -v yt-dlp >/dev/null; then
    printf '\nDo czatu bez klucza API zainstaluj yt-dlp.\nArch: sudo pacman -S yt-dlp\nUbuntu/Debian: sudo apt install yt-dlp\n'
fi
printf '\nGotowe — ChatHUD 0.6.1.\n'
printf 'Uruchom aplikację ChatHUD z menu lub: %s/.local/bin/chathud-gui\n' "$HOME"
printf 'Dotychczasowa konfiguracja została zachowana.\n'
printf 'Test bez internetu: %s/.local/bin/chathud --demo --no-game\n' "$HOME"
