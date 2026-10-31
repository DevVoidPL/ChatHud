"""Small PL/EN catalog. User chat, profile names and commands stay untouched."""
from __future__ import annotations
import json
import locale
from pathlib import Path
from core import atomic_write, config_dir

EN = {
 'TWÓJ CZAT. TWOJA GRA.': 'YOUR CHAT. YOUR GAME.',
 'PROFILE GIER': 'GAME PROFILES', 'Profil domyślny': 'Default profile',
 '+  Dodaj profil': '+  Add profile', 'Natywny overlay Linux': 'Native Linux overlay',
 'Twój czat w grze': 'Your in-game chat',
 'Ustaw raz. Zmieniaj live’a bez zmieniania komendy w Steam.': 'Save a profile. Change streams without changing your Steam command.',
 'Zapisz profil': 'Save profile', 'Zapisz zmiany •': 'Save changes •',
 '01  Transmisja': '01  Stream',
 'Wklej link do YouTube Live lub Twitch: https://www.twitch.tv/kanał': 'Paste a YouTube Live or Twitch URL: https://www.twitch.tv/channel',
 'YouTube: link do live’a. Twitch: link do kanału. Serwis rozpoznawany automatycznie.': 'YouTube: live stream URL. Twitch: channel URL. The service is detected automatically.',
 '02  Podgląd overlaya': '02  Overlay preview', 'Monitor': 'Monitor',
 'Główny monitor': 'Primary monitor', 'Odśwież listę monitorów': 'Refresh monitor list',
 'Podgląd orientacyjny. Test na pulpicie pokaże rzeczywisty rozmiar.': 'Illustrative preview. The desktop demo shows the actual size.',
 '▷  Test na pulpicie': '▷  Desktop demo', 'Połącz z live’em': 'Connect to chat',
 'Zatrzymaj test': 'Stop preview', 'Wygląd i pozycja': 'Appearance and position',
 'Lewy górny': 'Top left', 'Góra': 'Top center', 'Prawy górny': 'Top right',
 'Lewa strona': 'Center left', 'Środek': 'Center', 'Prawa strona': 'Center right',
 'Lewy dolny': 'Bottom left', 'Dół': 'Bottom center', 'Prawy dolny': 'Bottom right',
 'Pozycja': 'Position', 'Szerokość': 'Width', 'Tekst': 'Text', 'Wiadomości': 'Messages',
 'Krycie tła': 'Background opacity', 'Nazwa kanału w nagłówku': 'Show channel header',
 'Wygląd': 'Appearance', 'Zaawansowane': 'Advanced', 'Wyrównanie': 'Alignment',
 'Do lewej': 'Left', 'Do środka': 'Center', 'Do prawej': 'Right',
 'Najnowsze': 'Newest messages', 'Na dole': 'At the bottom', 'Na górze': 'At the top',
 'Margines': 'Margin', 'Nagłówek': 'Header', 'Rozmiar emotek': 'Emote size',
 'Odświeżanie': 'Refresh interval', 'Własne emotki YouTube': 'YouTube custom emotes',
 'Źródło YouTube': 'YouTube backend', 'Automatycznie': 'Automatic',
 'Tylko dla YouTube API': 'Only for the YouTube API backend', 'Klucz API': 'API key',
 '03  Opcje uruchamiania Steam': '03  Steam Launch Options', 'Dodaj MangoHud': 'Add MangoHud',
 'Steam → gra → Właściwości → Ogólne → Opcje uruchamiania': 'Steam → game → Properties → General → Launch Options',
 'Kopiuj komendę': 'Copy command',
 'Jeśli masz już własne opcje uruchamiania, połącz je ręcznie przed wklejeniem.': 'If you already use custom launch options, merge them before pasting.',
 'Diagnostyka': 'Diagnostics', 'Folder profili': 'Profiles folder',
 'Nie można odczytać ekranów X11/XWayland.': 'Could not read X11/XWayland displays.',
 'Brak aktywnych ekranów X11/XWayland.': 'No active X11/XWayland displays.',
 'Nieprawidłowe dane monitora.': 'Invalid monitor information.',
 ' Zapisany wybór zostaje zachowany.': ' Your saved selection is preserved.',
 'Pozycja czatu jest liczona tylko w obrębie tego monitora. Odłączony ekran zastępuje monitor główny.': 'Chat is positioned within this display. Disconnected displays fall back to the primary monitor.',
 'Gotowe do konfiguracji. ': 'Ready to configure. ',
 'Brak yt-dlp — szczegóły w Diagnostyce.': 'yt-dlp is missing (YouTube only). See Diagnostics.',
 'yt-dlp jest dostępny.': 'yt-dlp is available.',
 'Niezapisane zmiany. Komenda zostanie zapisana przed skopiowaniem.': 'Unsaved changes. Copying the command saves this profile first.',
 'Profil zapisany. Zmiany zadziałają przy następnym uruchomieniu overlaya.': 'Profile saved. Changes apply when the overlay starts again.',
 'Niezapisane zmiany': 'Unsaved changes', 'Zapisać zmiany w bieżącym profilu?': 'Save changes to the current profile?',
 'Zapisz': 'Save', 'Odrzuć': 'Discard', 'Anuluj': 'Cancel', 'OK': 'OK',
 'Nowy profil gry': 'New game profile', 'Wybierz grę Steam lub wpisz własną nazwę.': 'Choose a Steam game or enter a profile name.',
 'Nazwa gry / profilu': 'Game / profile name', 'Każdy profil ma własny link i wygląd czatu.': 'Each profile has its own chat URL and appearance.',
 'Najpierw wklej link do YouTube Live lub Twitch. Do sprawdzenia wyglądu użyj testu na pulpicie.': 'Paste a YouTube Live or Twitch URL first. Use the desktop demo to preview the layout.',
 'Brakuje overlaya lub launchera. W katalogu projektu uruchom ./install.sh.': 'The overlay or launcher is missing. Run ./install.sh from the source folder.',
 'MangoHud nie jest dostępny. Zainstaluj go lub wyłącz opcję „Dodaj MangoHud”.': 'MangoHud is unavailable. Install it or disable Add MangoHud.',
 'Skopiowano. Wklej komendę w opcjach uruchamiania wybranej gry w Steam.': 'Copied. Paste the command into this game’s Steam Launch Options.',
 'Brak programu chathud. W katalogu projektu uruchom make lub ./install.sh.': 'chathud is missing. Run make or ./install.sh from the source folder.',
 'Overlay wymaga X11 lub XWayland (DISPLAY). Panel ustawień działa także bez niego.': 'The overlay needs X11 or XWayland (DISPLAY). The settings app can run without it.',
 'Wklej link do YouTube Live lub Twitch.': 'Paste a YouTube Live or Twitch URL.',
 'Brak yt-dlp. Arch: sudo pacman -S yt-dlp\nUbuntu / Debian: sudo apt install yt-dlp\nMożesz też wybrać YouTube API i podać klucz.': 'yt-dlp is missing. Arch: sudo pacman -S yt-dlp\nUbuntu / Debian: sudo apt install yt-dlp\nYou can also select YouTube API and enter a key. Twitch does not need yt-dlp.',
 'Test na pulpicie działa. Użyj przycisku ■, aby zatrzymać overlay.': 'Preview is running. Press ■ to stop the overlay.',
 'Test zakończony.': 'Preview stopped.',
 'Overlay używa X11/XWayland. Widoczność nad grami Wayland zależy od kompozytora.': 'The overlay uses X11/XWayland. Visibility above Wayland games depends on the compositor.',
 'BRAK': 'MISSING', 'niewykryty (opcjonalny)': 'not detected (optional)', 'nieznana': 'unknown',
 'BRAK — overlay wymaga X11/XWayland': 'MISSING — the overlay needs X11/XWayland',
 'BRAK — sudo pacman -S yt-dlp (Arch) / sudo apt install yt-dlp (Debian/Ubuntu)': 'MISSING — sudo pacman -S yt-dlp (Arch) / sudo apt install yt-dlp (Debian/Ubuntu)',
 'ChatHUD — diagnostyka': 'ChatHUD — diagnostics', 'ChatHUD — błąd konfiguracji': 'ChatHUD — configuration error',
 'SCENA POGLĄDOWA': 'ILLUSTRATIVE SCENE', 'czat na żywo': 'live chat',
 'Siema! Jak wam leci?': 'Hey! How is everyone?', 'Ten widok robi robotę': 'What a view!',
 'Jeszcze jedna runda?': 'One more round?', 'Czat zawsze pod ręką': 'Chat right where you need it', 'Powodzenia!': 'Good luck!',
 'Link jest zbyt długi lub zawiera spacje.': 'The URL is too long or contains whitespace.',
 'Wklej pełny link https:// do YouTube Live lub kanału Twitch.': 'Paste a full https:// URL for YouTube Live or a Twitch channel.',
 'Nieprawidłowy port w linku.': 'Invalid port in the URL.',
 'Wklej link do kanału Twitch: https://www.twitch.tv/nazwa_kanału': 'Paste a Twitch channel URL: https://www.twitch.tv/channel_name',
 'Użyj linku do YouTube Live lub kanału Twitch: twitch.tv/nazwa_kanału': 'Use a YouTube Live or Twitch channel URL: twitch.tv/channel_name',
 'Ustawienia nie mogą zawierać znaków nowego wiersza.': 'Settings cannot contain newline characters.',
 'Nieprawidłowy plik profiles.json; zachowano go bez zmian.': 'Invalid profiles.json; the file has been preserved.',
 'Nieprawidłowy identyfikator profilu.': 'Invalid profile ID.', 'Nieprawidłowa nazwa profilu.': 'Invalid profile name.',
 'Brakuje profilu domyślnego.': 'The default profile is missing.',
 'Nazwa powinna mieć od 1 do 100 znaków.': 'The name must contain between 1 and 100 characters.',
 'Nie można zapisać wybranego języka.': 'Could not save the language preference.',
}
LANGUAGE = 'en'
# Deliberate fragments only for version strings, display metadata and diagnostics.
FRAGMENTS = {
 ' — centrum ustawień': ' — settings', 'Wersja ': 'Version ',
 ' (główny)': ' (primary)', ' (zapisany monitor)': ' (saved display)',
 ' (odłączony — użyty będzie główny)': ' (disconnected — using primary)',
 'Overlay zakończył się z błędem (': 'Overlay exited with an error (',
 'Sesja: ': 'Session: ', 'Profil: ': 'Profile: ', 'Log Steam: ': 'Steam log: ',
 'Nie można odczytać profili. Pliki pozostawiono bez zmian.': 'Could not read profiles. Existing files were preserved.',
}


def set_language(language):
    global LANGUAGE
    LANGUAGE = language if language in ('pl', 'en') else 'en'


def tr(text):
    if not isinstance(text, str):
        return text
    if LANGUAGE == 'en':
        if text in EN:
            return EN[text]
        for source, target in FRAGMENTS.items():
            text = text.replace(source, target)
        # Joined diagnostic/status lines can contain several exact sentences.
        for source, target in EN.items():
            if len(source) > 14:
                text = text.replace(source, target)
        return text
    reverse = {value: key for key, value in EN.items()}
    if text in reverse:
        return reverse[text]
    for source, target in FRAGMENTS.items():
        text = text.replace(target, source)
    for source, target in EN.items():
        if len(target) > 14:
            text = text.replace(target, source)
    return text


def load_language():
    try:
        data = json.loads((config_dir() / 'ui.json').read_text(encoding='utf-8'))
        if isinstance(data, dict) and data.get('language') in ('pl', 'en'):
            return data['language']
    except (OSError, ValueError, TypeError):
        pass
    return 'pl' if (locale.getlocale()[0] or '').lower().startswith('pl') else 'en'


def save_language(language):
    path = config_dir() / 'ui.json'
    data = {}
    if path.exists():
        data = json.loads(path.read_text(encoding='utf-8'))
        if not isinstance(data, dict):
            raise ValueError('Nie można zapisać wybranego języka.')
    data['language'] = language
    atomic_write(path, json.dumps(data, ensure_ascii=False, indent=2) + '\n')
