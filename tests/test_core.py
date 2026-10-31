from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'gui'))
from core import DEFAULTS, ProfileStore, launch_options, read_config, steam_games, validate_url, write_config

URL = 'https://www.youtube.com/watch?v=abcdefghijk&feature=share'


class CoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='chathud-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def test_preserve_comments_unknown_alias_and_private_permissions(self):
        path = self.root / 'config'
        path.write_text('# moja konfiguracja\nfuture_setting=keep\nopacity=0.2\nfont_size=12\nfont_size=13\n')
        values = dict(DEFAULTS, stream_url=URL, font_size='22', background_opacity='0.37')
        write_config(path, values)
        result = path.read_text()
        self.assertIn('# moja konfiguracja', result)
        self.assertIn('future_setting=keep', result)
        self.assertNotIn('\nopacity=', result)
        self.assertEqual(read_config(path)['stream_url'], URL)
        self.assertEqual(read_config(path)['font_size'], '22')
        self.assertEqual(read_config(path)['background_opacity'], '0.37')
        self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_profiles_are_independent_and_reopen(self):
        store = ProfileStore(self.root)
        self.assertFalse(self.root.joinpath('config').exists())
        default = store.profiles[0]
        write_config(store.path(default), dict(DEFAULTS, font_size='14'))
        game = store.add('Gra ze spacjami', dict(DEFAULTS, font_size='25', stream_url=URL))
        game['mangohud'] = True
        store.save()
        reopened = ProfileStore(self.root)
        self.assertEqual(len(reopened.profiles), 2)
        self.assertEqual(read_config(store.path(default))['font_size'], '14')
        self.assertEqual(read_config(store.path(game))['font_size'], '25')
        self.assertTrue(reopened.profiles[1]['mangohud'])

    def test_corrupt_metadata_is_not_overwritten(self):
        path = self.root / 'profiles.json'
        for content in ['broken', '{"profiles":[]}', '{"profiles":[{"id":"../../oops","name":"x"}]}']:
            path.write_text(content)
            with self.assertRaises(ValueError):
                ProfileStore(self.root)
            self.assertEqual(path.read_text(), content)

    def test_url_normalization_validation_and_injection(self):
        self.assertEqual(validate_url(f'[Live]({URL})'), URL)
        self.assertEqual(validate_url(f'  <{URL}>  '), URL)
        self.assertEqual(validate_url('https://youtu.be/abcdefghijk'), 'https://youtu.be/abcdefghijk')
        self.assertEqual(validate_url('https://www.youtube.com/live/abcdefghijk'), 'https://www.youtube.com/live/abcdefghijk')
        self.assertEqual(validate_url(''), '')
        for value in ['--exec x', 'https://youtube.com.evil.org/watch?v=abcdefghijk', 'https://evil@youtube.com/watch?v=abcdefghijk', URL+'\nprovider=api', 'https://youtube.com/watch?v=x']:
            with self.subTest(value=value), self.assertRaises(ValueError):
                validate_url(value)

    def test_multiline_values_cannot_inject_config(self):
        with self.assertRaises(ValueError):
            write_config(self.root/'config', dict(DEFAULTS, stream_url=URL+'\nprovider=api'))
        self.assertFalse((self.root/'config').exists())

    def test_shell_arguments_with_spaces_and_metacharacters(self):
        wrapper = self.root / "folder ' and $dollar" / 'chathud-steam'
        config = self.root / 'gra;$(touch BAD).conf'
        command = launch_options(wrapper, config, True)
        self.assertEqual(shlex.split(command), [str(wrapper),'--config',str(config),'--','mangohud','%command%'])
        wrapper.parent.mkdir()
        wrapper.write_text('#!/usr/bin/python3\nimport sys,json\nprint(json.dumps(sys.argv[1:]))\n')
        wrapper.chmod(0o755)
        import json
        result = subprocess.run(['bash','-c',command], cwd=self.root, capture_output=True, text=True, check=True)
        self.assertEqual(json.loads(result.stdout), ['--config',str(config),'--','mangohud','%command%'])
        self.assertFalse((self.root/'BAD').exists())

    def test_steam_library_discovery_reads_external_library(self):
        steam = self.root / '.local/share/Steam/steamapps'
        steam.mkdir(parents=True)
        external = self.root / 'Dysk ze spacjami'
        (external/'steamapps').mkdir(parents=True)
        (steam/'libraryfolders.vdf').write_text(f'"libraryfolders" {{ "0" {{ "path" "{external}" }} }}')
        manifest = external/'steamapps/appmanifest_123.acf'
        manifest.write_text('"AppState" { "name" "Test Game" }')
        before = manifest.read_bytes()
        self.assertEqual(steam_games(self.root), ['Test Game'])
        self.assertEqual(manifest.read_bytes(), before)

    def test_native_loads_stream_url_and_explicit_url_takes_priority(self):
        path = self.root / 'config'
        write_config(path, dict(DEFAULTS, stream_url=URL))
        env = dict(os.environ, DISPLAY='', PATH='/usr/bin:/bin')
        result = subprocess.run([str(ROOT/'chathud'),'--config',str(path),'--no-game'], env=env, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn('DISPLAY', result.stderr)
        self.assertNotIn('Brak linku', result.stderr)
        result = subprocess.run([str(ROOT/'chathud'),'--config',str(path),'--url','','--no-game'], env=env, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        self.assertIn('Brak linku', result.stderr)

    def test_steam_preserves_game_environment_and_cleans_overlay_environment(self):
        overlay = self.root/'overlay'
        overlay.write_text('''#!/usr/bin/python3
import json,os,sys
from pathlib import Path
Path(os.environ['RESULT']).write_text(json.dumps({'args':sys.argv[1:],'library':os.environ.get('LD_LIBRARY_PATH'),'preload':os.environ.get('LD_PRELOAD')}))
''')
        overlay.chmod(0o755)
        result_file = self.root/'result.json'
        env = dict(os.environ, XDG_CACHE_HOME=str(self.root/'cache'), CHATHUD_BIN=str(overlay), RESULT=str(result_file), LD_LIBRARY_PATH='/steam/runtime/lib', LD_PRELOAD='libm.so.6')
        game = '''import os,time,sys
from pathlib import Path
for i in range(100):
 if Path(os.environ['RESULT']).exists(): break
 time.sleep(.01)
assert os.environ['LD_LIBRARY_PATH']=='/steam/runtime/lib'
assert os.environ['LD_PRELOAD']=='libm.so.6'
assert sys.argv[1]=='argument with spaces'
sys.exit(23)
'''
        command = [str(ROOT/'chathud-steam'),'--config',str(self.root/'game profile.conf'),'--',sys.executable,'-c',game,'argument with spaces']
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 23, result.stderr)
        import json
        output = json.loads(result_file.read_text())
        self.assertIsNone(output['library'])
        self.assertIsNone(output['preload'])
        self.assertIn(str(self.root/'game profile.conf'), output['args'])
        self.assertIn('--watch-pid', output['args'])
        overlay.write_text('#!/usr/bin/env bash\nexit 42\n')
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 23)
        env['CHATHUD_BIN'] = str(self.root/'missing')
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 23)


    @unittest.skipUnless(os.environ.get('DISPLAY'), 'needs X11/XWayland')
    def test_stalled_downloader_is_killed_and_reaped_on_shutdown(self):
        downloader = self.root/'yt-dlp'
        marker = self.root/'downloader.pid'
        downloader.write_text("#!/usr/bin/python3\nimport os,signal,time\nfrom pathlib import Path\nsignal.signal(signal.SIGTERM, signal.SIG_IGN)\nPath(os.environ['PID_MARKER']).write_text(str(os.getpid()))\ntime.sleep(60)\n")
        downloader.chmod(0o755)
        config = self.root/'config'
        write_config(config, dict(DEFAULTS, stream_url=URL, provider='yt-dlp'))
        env = dict(os.environ, PATH=str(self.root)+':/usr/bin:/bin', PID_MARKER=str(marker))
        process = subprocess.Popen([str(ROOT/'chathud'),'--config',str(config),'--worker'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic()+3
            while not marker.exists() and process.poll() is None and time.monotonic()<deadline:
                time.sleep(.02)
            if process.poll() is not None:
                output = process.communicate()[1]
                if 'DISPLAY' in output:
                    self.skipTest('X11 display not accessible in this test environment')
                self.fail(output)
            self.assertTrue(marker.exists())
            child_pid = int(marker.read_text())
            started = time.monotonic()
            process.terminate()
            process.communicate(timeout=3)
            self.assertEqual(process.returncode, 0)
            self.assertLess(time.monotonic()-started, 2.5)
            with self.assertRaises(ProcessLookupError):
                os.kill(child_pid, 0)
        finally:
            if process.poll() is None:
                process.kill()
                process.communicate()


if __name__ == '__main__':
    unittest.main()
