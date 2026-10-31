"""Install desktop entries with an exact PNG icon path and matching app ID."""
from pathlib import Path
import sys

APP_ID = 'io.github.DevVoidPL.ChatHUD'
prefix, data_home, template = map(Path, sys.argv[1:4])
prefix, data_home = prefix.expanduser().resolve(), data_home.expanduser().resolve()
executable = str(prefix / 'bin/chathud-gui')
for value in (executable, str(data_home)):
    if any(ord(char) < 32 for char in value):
        raise ValueError('Control characters are not allowed in installation paths')
quoted = executable.replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%')
content = template.read_text(encoding='utf-8')
content = content.replace('Exec=chathud-gui', f'Exec="{quoted}"')
content = content.replace(f'Icon={APP_ID}', f'Icon={data_home / "chathud/assets/chathud.png"}')
applications = data_home / 'applications'
applications.mkdir(parents=True, exist_ok=True)
(applications / f'{APP_ID}.desktop').write_text(content, encoding='utf-8')
# Existing pinned launchers keep working without showing a duplicate menu item.
(applications / 'chathud.desktop').write_text(content + 'NoDisplay=true\n', encoding='utf-8')
