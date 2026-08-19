#!/usr/bin/env bash
set -euo pipefail

printf '==> Buduję ChatHUD 0.5.1\n'
make clean >/dev/null 2>&1 || true
make -j"$(nproc)"

printf '==> Instaluję ~/.local/bin/chathud i chathud-steam\n'
make install

cat <<'TXT'

Gotowe — ChatHUD 0.5.1.

WAŻNE: stary ~/.config/chathud/config nie jest nadpisywany.
Pełny nowy przykład jest tutaj:
  ~/.config/chathud/config.example

Edytuj ustawienia:
  nano ~/.config/chathud/config

Test:
  chathud --demo --no-game

Steam Launch Options:
  /home/frostreed/.local/bin/chathud-steam --url "LINK_DO_LIVE" -- %command%

Przykład bez tła i z mniejszym tekstem w configu:
  font_size=15
  background_opacity=0
  position=top-right

Log Steam:
  ~/.cache/chathud/steam-wrapper.log
TXT
