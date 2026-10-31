"""Headless widget checks with isolated profiles; no Steam changes or network."""
from pathlib import Path
import os
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'gui'))
try:
    import app as panel
except SystemExit:
    panel = None


@unittest.skipIf(panel is None, 'PySide6 is optional for the overlay')
class GuiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = panel.QApplication.instance() or panel.QApplication([])
        cls.app.setStyle('Fusion')
        cls.app.setStyleSheet(panel.STYLE.replace('CHECK_ICON', str(ROOT/'assets/check.svg')))

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='chathud-ui-test-')
        self.addCleanup(self.temp.cleanup)
        env = patch.dict(os.environ, {'XDG_CONFIG_HOME': self.temp.name})
        env.start()
        self.addCleanup(env.stop)
        self.window = panel.Window()
        self.errors = []
        self.window.error = lambda error: self.errors.append(str(error))
        self.window.show()
        self.app.processEvents()
        def cleanup():
            self.window.dirty = False
            self.window.close()
            self.app.processEvents()
        self.addCleanup(cleanup)

    def test_edit_save_copy_and_switch_keep_profiles_separate(self):
        w = self.window
        w.url.setText('https://www.youtube.com/watch?v=abcdefghijk')
        w.fields['font_size'].setValue(26)
        w.opacity.setValue(0)
        w.position_group.button(8).click()
        self.assertTrue(w.dirty)
        w.copy_command()
        self.assertEqual(self.errors, [])
        self.assertFalse(w.dirty)
        copied = self.app.clipboard().text()
        self.assertIn('--config', copied)
        self.assertIn('%command%', copied)
        self.assertNotIn('--url', copied)
        from core import read_config, DEFAULTS
        values = read_config(w.store.path(w.current))
        self.assertEqual(values['font_size'], '26')
        self.assertEqual(values['position'], 'bottom-right')
        self.assertEqual(values['background_opacity'], '0.00')
        second = w.store.add('Druga gra', dict(DEFAULTS, font_size='14'))
        w.profiles.addItem(second['name'])
        w.profiles.setCurrentRow(1)
        self.assertEqual(w.fields['font_size'].value(), 14)
        self.assertEqual(w.url.text(), '')
        w.profiles.setCurrentRow(0)
        self.assertEqual(w.fields['font_size'].value(), 26)
        self.assertEqual(w.url.text(), values['stream_url'])

    def test_invalid_link_cannot_overwrite_profile(self):
        w = self.window
        w.url.setText('https://youtube.com.evil.example/watch?v=abcdefghijk')
        self.assertFalse(w.save())
        self.assertTrue(self.errors)
        self.assertFalse(w.store.path(w.current).exists())

    def test_preview_controls_change_without_writing_config(self):
        w = self.window
        for index in range(9):
            w.position_group.button(index).click()
            self.assertEqual(w.preview.values['position'], panel.POSITIONS[index][0])
        w.fields['show_header'].setChecked(False)
        w.fields['message_order'].setCurrentIndex(1)
        self.assertEqual(w.preview.values['show_header'], '0')
        self.assertEqual(w.preview.values['message_order'], 'newest-first')
        self.assertFalse(w.store.path(w.current).exists())
        self.assertFalse(w.grab().isNull())

    @unittest.skipUnless(os.environ.get('DISPLAY'), 'native demo needs X11/XWayland')
    def test_native_demo_start_stop_and_config_snapshot_cleanup(self):
        w = self.window
        w.start_overlay(True)
        deadline = time.monotonic()+3
        while w.process and w.process.state() == panel.QProcess.ProcessState.Starting and time.monotonic()<deadline:
            self.app.processEvents()
            time.sleep(.01)
        self.app.processEvents()
        if not w.process and any('DISPLAY' in error for error in self.errors):
            self.skipTest('X11 display not accessible in this test environment')
        self.assertIsNotNone(w.process, self.errors)
        path = w.preview_file
        self.assertTrue(path.exists())
        self.assertFalse(w.store.path(w.current).exists())
        w.stop_overlay()
        deadline = time.monotonic()+4
        while w.process and time.monotonic()<deadline:
            self.app.processEvents()
            time.sleep(.01)
        self.assertIsNone(w.process)
        self.assertFalse(path.parent.exists())
        self.assertTrue(w.demo_button.isEnabled())
        self.assertFalse(w.stop_button.isEnabled())
        self.assertFalse(self.errors)


if __name__ == '__main__':
    unittest.main()
