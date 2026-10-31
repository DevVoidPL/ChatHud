![ChatHUD](assets/chathud-banner.png)

[English](README.md) | [Historia zmian](CHANGELOG.md)

# ChatHUD

ChatHUD wyświetla czat YouTube Live i Twitcha nad grami w Linuksie. Overlay jest napisany w C, a osobna aplikacja Qt służy do ustawiania wyglądu, profili i komend Steam. Podczas gry panel ustawień może być zamknięty.

Wersja 0.7.0 (wersja alfa). Overlay korzysta z X11/XWayland. Program trzeba zbudować z kodu źródłowego według instrukcji poniżej. Paczki Flatpak i AppImage nie są jeszcze dostępne.

- Linki do YouTube Live i kanałów Twitch, rozpoznawane automatycznie.
- Osobny link, monitor i wygląd czatu dla każdej gry.
- Przepuszczanie kliknięć do gry, zawijanie wiadomości, rozmiary tekstu, krycie tła i dziewięć pozycji w obrębie wybranego monitora.
- Polski i angielski interfejs, przełączany bez restartu panelu.
- Launcher Steam/Proton i opcjonalna obsługa MangoHud.

![ChatHUD podczas gry](assets/chathud-ingame.png)

Zrzut z wcześniejszej wersji programu.

## Instalacja

### Zależności

Arch Linux / EndeavourOS:

```bash
sudo pacman -S --needed \
  git base-devel pkgconf python pyside6 qt6-svg \
  libx11 libxrandr libxfixes libxext cairo pango \
  curl json-c libjpeg-turbo libwebp yt-dlp
```

Debian 13 / Ubuntu 26.04:

```bash
sudo apt update
sudo apt install \
  git build-essential pkg-config python3 \
  python3-pyside6.qtwidgets python3-pyside6.qtsvg \
  libx11-dev libxrandr-dev libxfixes-dev libxext-dev \
  libcairo2-dev libpango1.0-dev libcurl4-openssl-dev \
  libjson-c-dev libjpeg-dev libwebp-dev yt-dlp
```

Pakiety PySide6 w Ubuntu są w repozytorium Universe. Na starszych wydaniach, które ich nie mają, skorzystaj z opisanego niżej środowiska Pythona. YouTube domyślnie korzysta z `yt-dlp`. Twitch łączy się przez libcurl.

### Pobranie i budowanie

```bash
git clone https://github.com/DevVoidPL/ChatHud.git
cd ChatHud
./install.sh
```

Uruchom instalator jako zwykły użytkownik. Buduje overlay i instaluje ChatHUD w `~/.local`, razem z wpisem w menu aplikacji. Istniejące ustawienia zostają zachowane.

Możesz też wybrać Code > Download ZIP, rozpakować archiwum i wykonać `bash install.sh` w rozpakowanym folderze. ZIP zawiera kod źródłowy, więc nadal potrzebujesz zależności do budowania.

Otwórz ChatHUD z menu aplikacji albo wykonaj:

```bash
~/.local/bin/chathud-gui
```

<details>
<summary>Starszy Debian / Ubuntu: osobne środowisko Pythona dla PySide6</summary>

Zainstaluj podane wyżej biblioteki i narzędzia, pomijając `python3-pyside6.qtwidgets` i `python3-pyside6.qtsvg`. Dodaj `python3-venv`, a następnie użyj Pythona 3.10 lub nowszego:

```bash
sudo apt install python3-venv
CHATHUD_VENV="${XDG_DATA_HOME:-$HOME/.local/share}/chathud/venv"
python3 -m venv "$CHATHUD_VENV"
"$CHATHUD_VENV/bin/python" -m pip install 'PySide6>=6.5,<7'
```

Launcher automatycznie korzysta z tego środowiska, również przy uruchamianiu z menu aplikacji. Jeśli go nie ma, używa systemowego Pythona.

</details>

## Ustawienie gry

1. W prawym górnym rogu wybierz Polski albo English. Aplikacja zapamięta język.
2. Wklej link do YouTube Live, np. `https://www.youtube.com/watch?v=VIDEO_ID`, lub kanału Twitch, np. `https://www.twitch.tv/nazwa_kanału`.
3. Wybierz monitor, pozycję czatu i ustaw wygląd.
4. Test na pulpicie pokazuje przykładowe wiadomości. Połącz z live'em otwiera podany czat. Kwadratowy przycisk zatrzymuje podgląd.
5. Kliknij Kopiuj komendę i wklej wynik w Steam > gra > Właściwości > Ogólne > Opcje uruchamiania.

Jeśli masz już własne opcje uruchamiania, połącz je ręcznie przed wklejeniem. Dla kolejnej gry użyj Dodaj profil. Link jest zapisany w profilu, więc przy zmianie transmisji wystarczy zapisać ten sam profil i uruchomić overlay ponownie. Komenda w Steam zostaje taka sama.

Przykład dla profilu domyślnego:

```bash
$HOME/.local/bin/chathud-steam --config "$HOME/.config/chathud/config" -- %command%
```

Z zainstalowanym MangoHud:

```bash
$HOME/.local/bin/chathud-steam --config "$HOME/.config/chathud/config" -- mangohud %command%
```

Overlay bez uruchamiania gry:

```bash
~/.local/bin/chathud --url "https://www.twitch.tv/nazwa_kanału" --no-game
```

## YouTube i Twitch

YouTube domyślnie korzysta z `yt-dlp`. Opcjonalny backend YouTube Data API wymaga własnego klucza. Obsługiwane są obrazkowe emotki YouTube.

Twitch korzysta z anonimowego odczytu IRC przez TLS. Obsługuje wiadomości, ponowne łączenie, podtrzymywanie połączenia i usuwanie wiadomości przez moderację. Nie wysyła wiadomości na czat. Wklej link do kanału. Linki do VOD i klipów nie są obsługiwane. Emotki Twitcha są na razie wyświetlane jako tekst. Obrazki emotek oraz BTTV/FFZ/7TV nie są jeszcze obsługiwane.

Backend jest wybierany na podstawie linku. Pole Źródło YouTube dotyczy tylko YouTube. Twitch nie potrzebuje `yt-dlp` ani klucza YouTube. Jeśli Twitch odrzuci anonimowe połączenie, overlay pokaże komunikat o błędzie.

## Ustawienia

- `~/.config/chathud/config`: profil domyślny.
- `~/.config/chathud/profiles/*.conf`: profile gier.
- `~/.config/chathud/profiles.json`: nazwy profili i wybór MangoHud.
- `~/.config/chathud/ui.json`: język interfejsu.
- `~/.cache/chathud/steam-wrapper.log`: log launchera Steam.

Ścieżki uwzględniają `XDG_CONFIG_HOME`, `XDG_DATA_HOME` i `XDG_CACHE_HOME`. Wszystkie opcje są w [config.example](config.example). Przykład:

```ini
stream_url=https://www.twitch.tv/channel_name
language=pl
monitor=primary
position=top-right
width=700
lines=6
font_size=18
background_opacity=0.62
```

`language` ustawia język komunikatów overlaya. Panel zapisuje tę wartość razem z profilem. Treść czatu nie jest tłumaczona. `background_opacity=0` usuwa ciemne tło panelu. Zmiany są wczytywane przy kolejnym uruchomieniu overlaya.

## W razie problemów

- Błąd budowania: sprawdź zależności, w tym pakiety deweloperskie z nagłówkami.
- Brak PySide6: zainstaluj pakiety Qt dystrybucji lub utwórz osobne środowisko opisane wyżej.
- Zły monitor: wybierz ekran, zapisz profil i uruchom overlay ponownie. Po odłączeniu wybranego ekranu używany jest główny monitor.
- Brak overlaya na Wayland: sprawdź XWayland i zmienną `DISPLAY`. Widoczność nad natywnymi grami Wayland zależy od kompozytora.
- Pusty czat YouTube: sprawdź link, dostępność czatu na transmisji i wersję `yt-dlp`.
- Pusty czat Twitch: sprawdź link do kanału i komunikat połączenia w overlayu.
- Problem w Steam: zajrzyj do logu launchera. Przycisk Diagnostyka pokazuje ścieżki i brakujące zależności.

Zgłaszając błąd, podaj dystrybucję, sesję X11/Wayland, wersję programu i istotny fragment logu. Usuń klucze API przed udostępnieniem logów.

## Aktualizacja i odinstalowanie

W kopii pobranej przez Git, bez własnych zmian w kodzie:

```bash
git pull --ff-only
./install.sh
```

Po aktualizacji otwórz panel i overlay ponownie. Przy instalacji z ZIP pobierz nowy kod i uruchom jego instalator.

Aby odinstalować program, wykonaj `make uninstall` w katalogu źródeł. Usuwa pliki aplikacji i opcjonalne środowisko Pythona. Profile i pamięć podręczna pozostają.

[Licencja MIT](LICENSE)
