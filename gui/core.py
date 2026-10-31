"""Storage and Steam command generation; no GUI or Steam writes."""
from __future__ import annotations

import json
import os
from pathlib import Path
import re
import shlex
import tempfile
from urllib.parse import urlparse, parse_qs
import uuid

DEFAULTS = {
    'stream_url': '', 'provider': 'auto', 'youtube_api_key': '',
    'lines': '6', 'width': '700', 'font_size': '18', 'header_font_size': '16',
    'show_header': '1', 'background_opacity': '0.62', 'margin': '24',
    'monitor': 'primary', 'position': 'top-right', 'text_align': 'left', 'message_order': 'oldest-first',
    'emotes': '1', 'emote_size': '24', 'refresh_ms': '100',
}
POSITIONS = [
    ('top-left', 'Lewy górny'), ('top-center', 'Góra'), ('top-right', 'Prawy górny'),
    ('center-left', 'Lewa strona'), ('center', 'Środek'), ('center-right', 'Prawa strona'),
    ('bottom-left', 'Lewy dolny'), ('bottom-center', 'Dół'), ('bottom-right', 'Prawy dolny'),
]


def config_dir() -> Path:
    return Path(os.environ.get('XDG_CONFIG_HOME') or Path.home() / '.config') / 'chathud'


def normalize_url(value: str) -> str:
    value = value.strip()
    match = re.fullmatch(r'\[[^\n]*\]\((https?://[^\s]+)\)', value)
    if match:
        value = match[1]
    return value.removeprefix('<').removesuffix('>').strip()


def validate_url(value: str) -> str:
    value = normalize_url(value)
    if not value:
        return ''
    if len(value.encode('utf-8')) > 2047 or any(c.isspace() for c in value):
        raise ValueError('Link jest zbyt długi lub zawiera spacje.')
    parsed = urlparse(value)
    if parsed.scheme not in ('http', 'https') or parsed.username or parsed.password:
        raise ValueError('Wklej pełny link https:// do transmisji YouTube Live.')
    host = (parsed.hostname or '').lower()
    valid = False
    if host in ('youtu.be', 'www.youtu.be'):
        valid = bool(re.fullmatch(r'/[A-Za-z0-9_-]{11}/?', parsed.path))
    elif host in ('youtube.com', 'www.youtube.com', 'm.youtube.com', 'music.youtube.com'):
        video = parse_qs(parsed.query).get('v', [''])[0]
        valid = (parsed.path == '/watch' and bool(re.fullmatch(r'[A-Za-z0-9_-]{11}', video)))
        valid |= bool(re.fullmatch(r'/(?:live|shorts)/[A-Za-z0-9_-]{11}/?', parsed.path))
    if not valid:
        raise ValueError('Użyj linku do konkretnego live’a: youtube.com/watch?v=… lub youtu.be/…')
    return value


def read_config(path: Path) -> dict[str, str]:
    values = dict(DEFAULTS)
    if not path.exists():
        return values
    for line in path.read_text(encoding='utf-8').splitlines():
        key, sep, value = line.strip().partition('=')
        if sep and not key.startswith('#'):
            key, value = key.strip(), value.strip()
            values['background_opacity' if key == 'opacity' else key] = value
    return values


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f'.{path.name}.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            stream.write(content)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def write_config(path: Path, values: dict[str, str]) -> None:
    """Preserve comments/unknown settings; replace all known duplicate keys."""
    for key, value in values.items():
        if '\n' in value or '\r' in value or '\n' in key:
            raise ValueError('Ustawienia nie mogą zawierać znaków nowego wiersza.')
    existing = path.read_text(encoding='utf-8').splitlines() if path.exists() else ['# ChatHUD 0.6.0']
    output, seen = [], set()
    for line in existing:
        key, sep, _ = line.strip().partition('=')
        key = key.strip()
        canonical = 'background_opacity' if key == 'opacity' else key
        if sep and not key.startswith('#') and canonical in values:
            output.append(f'{canonical}={values[canonical]}')
            seen.add(canonical)
        else:
            output.append(line)
    output.extend(f'{key}={value}' for key, value in values.items() if key not in seen)
    atomic_write(path, '\n'.join(output) + '\n')


class ProfileStore:
    def __init__(self, root: Path | None = None):
        self.root = root or config_dir()
        self.metadata = self.root / 'profiles.json'
        self.profiles = [{'id': 'default', 'name': 'Profil domyślny', 'mangohud': False}]
        if self.metadata.exists():
            data = json.loads(self.metadata.read_text(encoding='utf-8'))
            profiles = data['profiles']
            if not isinstance(profiles, list) or not profiles:
                raise ValueError('Nieprawidłowy plik profiles.json; zachowano go bez zmian.')
            ids = set()
            for profile in profiles:
                ident = profile['id']
                if not isinstance(ident, str) or not re.fullmatch(r'default|[0-9a-f]{32}', ident) or ident in ids:
                    raise ValueError('Nieprawidłowy identyfikator profilu.')
                if not isinstance(profile['name'], str) or not profile['name'].strip():
                    raise ValueError('Nieprawidłowa nazwa profilu.')
                ids.add(ident)
            if 'default' not in ids:
                raise ValueError('Brakuje profilu domyślnego.')
            self.profiles = profiles

    def path(self, profile: dict) -> Path:
        return self.root / 'config' if profile['id'] == 'default' else self.root / 'profiles' / f"{profile['id']}.conf"

    def save(self) -> None:
        atomic_write(self.metadata, json.dumps({'version': 1, 'profiles': self.profiles}, ensure_ascii=False, indent=2) + '\n')

    def add(self, name: str, values: dict[str, str]) -> dict:
        name = name.strip()
        if not name or len(name) > 100 or any(ord(c) < 32 for c in name):
            raise ValueError('Nazwa powinna mieć od 1 do 100 znaków.')
        profile = {'id': uuid.uuid4().hex, 'name': name, 'mangohud': False}
        write_config(self.path(profile), values)
        self.profiles.append(profile)
        try:
            self.save()
        except Exception:
            self.profiles.pop()
            self.path(profile).unlink(missing_ok=True)
            raise
        return profile


def launch_options(wrapper: Path, config: Path, mangohud: bool = False) -> str:
    return f'{shlex.quote(str(wrapper))} --config {shlex.quote(str(config))} -- ' + ('mangohud ' if mangohud else '') + '%command%'


def steam_games(home: Path | None = None) -> list[str]:
    """Read installed game names only; never alter Steam's localconfig.vdf."""
    home = home or Path.home()
    roots = [home / '.local/share/Steam', home / '.steam/steam', home / '.steam/root',
             home / '.var/app/com.valvesoftware.Steam/.local/share/Steam']
    libraries = set()
    for root in roots:
        libraries.add(root.resolve())
        vdf = root / 'steamapps/libraryfolders.vdf'
        try:
            for path in re.findall(r'"path"\s+"((?:\\.|[^"\\])*)"', vdf.read_text(encoding='utf-8')):
                libraries.add(Path(path.replace('\\\\', '\\').replace('\\"', '"')))
        except (OSError, UnicodeError):
            pass
    names = set()
    for library in libraries:
        for manifest in (library / 'steamapps').glob('appmanifest_*.acf'):
            try:
                match = re.search(r'"name"\s+"((?:\\.|[^"\\])*)"', manifest.read_text(encoding='utf-8'))
                if match:
                    names.add(match[1].replace('\\"', '"').replace('\\\\', '\\'))
            except (OSError, UnicodeError):
                pass
    return sorted(names, key=str.casefold)
