#!/usr/bin/env python3
"""ChatHUD desktop configuration panel. Qt is used only by the editor."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import sys

try:
    from PySide6.QtCore import Qt, QRectF, QPointF, QProcess, QProcessEnvironment, QTimer, QUrl
    from PySide6.QtGui import QColor, QDesktopServices, QFont, QIcon, QLinearGradient, QPainter, QPainterPath, QPen, QTextDocument
    from PySide6.QtWidgets import (
        QApplication, QButtonGroup, QCheckBox, QComboBox, QDialog, QDialogButtonBox,
        QFormLayout, QFrame, QGridLayout, QHBoxLayout, QLabel, QLineEdit, QListWidget,
        QMessageBox, QPushButton, QScrollArea, QSlider, QSpinBox, QTabWidget,
        QTextEdit, QVBoxLayout, QWidget,
    )
except ImportError:
    sys.exit('ChatHUD GUI requires / wymaga PySide6. Arch: sudo pacman -S pyside6 qt6-svg | Debian/Ubuntu: sudo apt install python3-pyside6.qtwidgets python3-pyside6.qtsvg')

from core import DEFAULTS, POSITIONS, ProfileStore, launch_options, read_config, steam_games, validate_url, write_config, stream_platform

from i18n import tr, set_language, load_language, save_language

ROOT = Path(__file__).resolve().parent.parent
VERSION = '0.7.0'
APP_ID = 'io.github.DevVoidPL.ChatHUD'
STYLE = '''
QWidget { color: #e6ebf6; font-family: "Inter", "Noto Sans", sans-serif; font-size: 13px; }
QWidget#window { background: #0d1017; }
QFrame#sidebar { background: #131823; border-right: 1px solid #252e40; }
QFrame#card { background: #151b27; border: 1px solid #283348; border-radius: 14px; }
QLabel { background: transparent; }
QLabel#brand { font-size: 25px; font-weight: 800; letter-spacing: 1px; color: #f4f7ff; }
QLabel#heading { font-size: 26px; font-weight: 700; }
QLabel#section { font-size: 15px; font-weight: 650; }
QLabel#muted { color: #92a1b8; font-size: 12px; }
QLabel#eyebrow { color: #69dfb0; font-size: 11px; font-weight: 700; letter-spacing: 2px; }
QListWidget { border: none; background: transparent; outline: none; }
QListWidget::item { padding: 13px 11px; margin: 3px 0; border-radius: 8px; color: #aab7cc; }
QListWidget::item:selected { background: #21392f; color: #94f1c6; }
QListWidget::item:hover { background: #20293a; }
QLineEdit, QComboBox, QSpinBox, QTextEdit { background: #0e1420; border: 1px solid #344157; border-radius: 7px; padding: 8px; selection-background-color: #306f57; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border: 1px solid #70dfb1; }
QLineEdit[invalid="true"] { border: 1px solid #f5958f; }
QComboBox::drop-down, QSpinBox::up-button, QSpinBox::down-button { border: none; width: 23px; }
QComboBox QAbstractItemView { background: #1b2534; selection-background-color: #315b49; padding: 5px; }
QPushButton { background: #253044; border: 1px solid #3a4860; padding: 10px 15px; border-radius: 8px; font-weight: 600; }
QPushButton:hover { background: #33425b; border-color: #586c8a; }
QPushButton:pressed { background: #192335; }
QPushButton#primary { background: #80edba; color: #10291e; border: none; }
QPushButton#primary:hover { background: #a6f5d0; }
QPushButton#quiet { background: transparent; border: 1px solid #344157; color: #aebdd0; }
QPushButton#position { padding: 6px; font-size: 18px; background: #101723; border: 1px solid #36445b; }
QPushButton#position:checked { background: #244b3a; color: #a0f7cb; border-color: #80edba; }
QPushButton:disabled { background: #1c2432; color: #59677b; border-color: #2a3446; }
QCheckBox { spacing: 9px; padding: 5px 0; }
QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #53627b; border-radius: 4px; background: #111824; }
QCheckBox::indicator:checked { background: #80edba; border-color: #80edba; image: url(CHECK_ICON); }
QSlider::groove:horizontal { background: #2c3a4e; height: 5px; border-radius: 2px; }
QSlider::sub-page:horizontal { background: #80edba; border-radius: 2px; }
QSlider::handle:horizontal { background: #d4ffe8; width: 15px; margin: -5px 0; border-radius: 7px; }
QTabWidget::pane { border: none; padding-top: 12px; }
QTabBar::tab { padding: 9px 14px; color: #91a2b9; background: transparent; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: #9cf2c8; border-bottom-color: #80edba; }
QScrollArea { background: transparent; border: none; }
QScrollBar:vertical { background: #111721; width: 7px; }
QScrollBar::handle:vertical { background: #36455c; border-radius: 3px; min-height: 24px; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QToolTip { color: #f1f5ff; background: #253044; border: 1px solid #516581; padding: 6px; }
'''


def bind(widget, kind, source):
    widget.setProperty('i18n_' + kind, source)
    getattr(widget, 'set' + kind[0].upper() + kind[1:])(tr(source))


def checkbox(text):
    item = QCheckBox()
    bind(item, 'text', text)
    return item


def add_combo_item(widget, source, value=None):
    widget.addItem(tr(source), value)
    widget.setItemData(widget.count()-1, source, int(Qt.ItemDataRole.UserRole)+1)


def add_tab(tabs, widget, source):
    widget.setProperty('i18n_tab', source)
    tabs.addTab(widget, tr(source))


def label(text, style=None):
    item = QLabel()
    bind(item, 'text', text)
    item.setTextFormat(Qt.TextFormat.PlainText)
    if style:
        item.setObjectName(style)
    return item


def button(text, action, style=None):
    item = QPushButton()
    bind(item, 'text', text)
    if style:
        item.setObjectName(style)
    item.clicked.connect(action)
    return item


def card():
    item = QFrame()
    item.setObjectName('card')
    layout = QVBoxLayout(item)
    layout.setContentsMargins(20, 14, 20, 14)
    layout.setSpacing(10)
    return item, layout


class Preview(QWidget):
    """Illustrative scene. Use the native demo to check actual pixel sizes."""
    def __init__(self):
        super().__init__()
        self.values = dict(DEFAULTS)
        self.setMinimumHeight(200)
        self.setMinimumWidth(330)

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        rect = QRectF(self.rect()).adjusted(1, 1, -1, -1)
        clip = QPainterPath()
        clip.addRoundedRect(rect, 10, 10)
        p.setClipPath(clip)
        gradient = QLinearGradient(rect.topLeft(), rect.bottomRight())
        gradient.setColorAt(0, QColor('#223f51'))
        gradient.setColorAt(.55, QColor('#142a38'))
        gradient.setColorAt(1, QColor('#0a1724'))
        p.fillRect(rect, gradient)
        w, h = rect.width(), rect.height()
        # Original vector landscape, with no downloaded artwork.
        for color, points in [('#31505e', [(0,.64),(.15,.29),(.26,.51),(.43,.2),(.63,.65),(.84,.35),(1,.62)]),
                              ('#1b3442', [(0,.79),(.24,.56),(.41,.82),(.64,.43),(.87,.75),(1,.59)]),
                              ('#112633', [(0,.85),(.17,.74),(.44,.91),(.68,.71),(1,.87)])]:
            path = QPainterPath(QPointF(0, h))
            for x, y in points:
                path.lineTo(x*w, y*h)
            path.lineTo(w, h)
            path.closeSubpath()
            p.fillPath(path, QColor(color))
        p.setPen(QPen(QColor('#8cb6be'), 1))
        p.drawLine(QPointF(w/2-7,h/2), QPointF(w/2+7,h/2))
        p.drawLine(QPointF(w/2,h/2-7), QPointF(w/2,h/2+7))
        v = self.values
        # Readable illustration; dimensions deliberately scale less than the game.
        panel_w = min(w-24, max(170, int(v['width'])*.41))
        font_px = max(9, min(24, int(v['font_size'])*.6))
        alignment = {'left': 'left', 'center': 'center', 'right': 'right'}[v['text_align']]
        messages = [
            ('DevVoidPL', 'Siema! Jak wam leci?'), ('PixelCat', 'Ten widok robi robotę'),
            ('LinuxEnjoyer', 'ChatHUD + MangoHud ♥'), ('Karton', 'Jeszcze jedna runda?'),
            ('BarryFan', 'Czat zawsze pod ręką'), ('Ada', 'Powodzenia!'), ('Neo', 'GG!'),
        ][-min(7, int(v['lines'])):]
        if v['message_order'] == 'newest-first':
            messages.reverse()
        body = ''
        if v['show_header'] == '1':
            platform = 'Twitch' if stream_platform(v.get('stream_url', '')) == 'twitch' else 'YouTube'
            body += f'<p style="color:#bed7ff;font-size:{max(9,int(v["header_font_size"])*.6)}px;margin:0 0 7px">{platform} · {tr("czat na żywo")}</p>'
        for author, text in messages:
            body += f'<p style="margin:0 0 5px"><b>{author}:</b> {tr(text)}</p>'
        doc = QTextDocument()
        doc.setDefaultFont(QFont('Noto Sans', 10))
        doc.setDefaultStyleSheet(f'body {{color:#f1f5ff;font-size:{font_px}px;text-align:{alignment}}}')
        doc.setHtml('<body>'+body+'</body>')
        doc.setTextWidth(panel_w-20)
        panel_h = min(h-24, doc.size().height()+16)
        margin = min(30, max(10, int(v['margin'])*.35))
        pos = v['position']
        x = margin if 'left' in pos else w-panel_w-margin if 'right' in pos else (w-panel_w)/2
        y = margin if 'top' in pos else h-panel_h-margin if 'bottom' in pos else (h-panel_h)/2
        p.setPen(Qt.PenStyle.NoPen)
        p.setBrush(QColor(8, 8, 8, round(float(v['background_opacity'])*255)))
        p.drawRoundedRect(QRectF(x, y, panel_w, panel_h), 8, 8)
        p.save()
        p.setClipRect(QRectF(x+6,y+6,panel_w-12,panel_h-12))
        p.translate(x+10, y+8)
        doc.drawContents(p)
        p.restore()
        p.setPen(QColor('#7d9cae'))
        p.setFont(QFont('Noto Sans', 8))
        p.drawText(QRectF(12,h-23,180,17), tr('SCENA POGLĄDOWA'))


class Window(QWidget):
    def __init__(self):
        super().__init__()
        self.language = load_language()
        set_language(self.language)
        self.store = ProfileStore()
        self.current = self.store.profiles[0]
        self.dirty = False
        self.loading = True
        self.process = None
        self.stopping = False
        self.preview_file = None
        self.monitor_query = None
        self.setObjectName('window')
        bind(self, 'windowTitle', f'ChatHUD {VERSION} — centrum ustawień')
        self.setWindowIcon(QIcon(str(ROOT / 'assets/chathud.png')))
        self.resize(1240, 980)
        self.setMinimumSize(990, 740)
        outer = QHBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        sidebar = QFrame()
        sidebar.setObjectName('sidebar')
        sidebar.setFixedWidth(218)
        side = QVBoxLayout(sidebar)
        side.setContentsMargins(18, 28, 18, 22)
        brand_row = QHBoxLayout()
        brand_row.setSpacing(8)
        brand_icon = QLabel()
        brand_icon.setPixmap(QIcon(str(ROOT / 'assets/chathud.png')).pixmap(32, 32))
        brand_row.addWidget(brand_icon)
        brand_row.addWidget(label('ChatHUD', 'brand'), 1)
        side.addLayout(brand_row)
        side.addWidget(label('TWÓJ CZAT. TWOJA GRA.', 'eyebrow'))
        side.addSpacing(34)
        side.addWidget(label('PROFILE GIER', 'muted'))
        self.profiles = QListWidget()
        self.profiles.setWordWrap(True)
        self.profiles.addItems([tr('Profil domyślny') if p['id'] == 'default' else p['name'] for p in self.store.profiles])
        self.profiles.setCurrentRow(0)
        self.profiles.currentRowChanged.connect(self.switch_profile)
        side.addWidget(self.profiles, 1)
        side.addWidget(button('+  Dodaj profil', self.add_profile, 'quiet'))
        side.addSpacing(20)
        side.addWidget(label('Natywny overlay Linux', 'muted'))
        side.addWidget(label(f'Wersja {VERSION}  /  YouTube + Twitch', 'muted'))
        outer.addWidget(sidebar)
        area = QScrollArea()
        area.setWidgetResizable(True)
        content = QWidget()
        content.setObjectName('window')
        main = QVBoxLayout(content)
        main.setContentsMargins(28, 25, 28, 20)
        main.setSpacing(14)
        top = QHBoxLayout()
        titles = QVBoxLayout()
        titles.setSpacing(4)
        self.heading = label('Twój czat w grze', 'heading')
        titles.addWidget(self.heading)
        titles.addWidget(label('Ustaw raz. Zmieniaj live’a bez zmieniania komendy w Steam.', 'muted'))
        top.addLayout(titles, 1)
        self.language_switch = QComboBox()
        self.language_switch.setFixedWidth(105)
        self.language_switch.addItem('Polski', 'pl')
        self.language_switch.addItem('English', 'en')
        self.language_switch.setCurrentIndex(self.language_switch.findData(self.language))
        bind(self.language_switch, 'accessibleName', 'Language / Język')
        self.language_switch.currentIndexChanged.connect(self.change_language)
        top.addWidget(self.language_switch)
        self.save_button = button('Zapisz profil', self.save, 'primary')
        top.addWidget(self.save_button)
        main.addLayout(top)

        stream, stream_layout = card()
        stream_layout.addWidget(label('01  Transmisja', 'section'))
        self.url = QLineEdit()
        bind(self.url, 'placeholderText', 'Wklej link do YouTube Live lub Twitch: https://www.twitch.tv/kanał')
        self.url.setMaxLength(2047)
        self.url.setClearButtonEnabled(True)
        self.url.textChanged.connect(self.changed)
        stream_layout.addWidget(self.url)
        stream_layout.addWidget(label('YouTube: link do live’a. Twitch: link do kanału. Serwis rozpoznawany automatycznie.', 'muted'))
        main.addWidget(stream)
        columns = QHBoxLayout()
        columns.setSpacing(17)
        preview_card, preview_layout = card()
        preview_card.setMaximumHeight(450)
        preview_layout.addWidget(label('02  Podgląd overlaya', 'section'))
        monitor_row = QHBoxLayout()
        monitor_row.addWidget(label('Monitor'))
        self.monitor = QComboBox()
        self.monitor.setFixedHeight(32)
        add_combo_item(self.monitor, 'Główny monitor', 'primary')
        self.monitor.currentIndexChanged.connect(self.changed)
        monitor_row.addWidget(self.monitor, 1)
        refresh_monitor = button('↻', self.refresh_monitors, 'quiet')
        refresh_monitor.setFixedWidth(36)
        bind(refresh_monitor, 'toolTip', 'Odśwież listę monitorów')
        monitor_row.addWidget(refresh_monitor)
        preview_layout.addLayout(monitor_row)
        self.preview = Preview()
        preview_layout.addWidget(self.preview, 1)
        preview_layout.addWidget(label('Podgląd orientacyjny. Test na pulpicie pokaże rzeczywisty rozmiar.', 'muted'))
        controls = QHBoxLayout()
        self.demo_button = button('▷  Test na pulpicie', lambda: self.start_overlay(True))
        self.live_button = button('Połącz z live’em', lambda: self.start_overlay(False))
        controls.addWidget(self.demo_button)
        controls.addWidget(self.live_button)
        preview_layout.addLayout(controls)
        self.stop_button = button('■', self.stop_overlay, 'quiet')
        bind(self.stop_button, 'toolTip', 'Zatrzymaj test')
        bind(self.stop_button, 'accessibleName', 'Zatrzymaj test')
        self.stop_button.setFixedWidth(44)
        self.stop_button.setEnabled(False)
        controls.addWidget(self.stop_button)
        columns.addWidget(preview_card, 5)
        settings, settings_layout = card()
        settings.setMaximumHeight(450)
        settings.setMinimumWidth(340)
        settings.setMaximumWidth(400)
        settings_layout.addWidget(label('Wygląd i pozycja', 'section'))
        tabs = QTabWidget()
        appearance = QWidget()
        form = QFormLayout(appearance)
        form.setContentsMargins(0, 2, 0, 0)
        form.setSpacing(9)
        self.fields = {'monitor': self.monitor}
        self.position_group = QButtonGroup(self)
        grid = QGridLayout()
        arrows = ['↖', '↑', '↗', '←', '•', '→', '↙', '↓', '↘']
        for index, (key, title) in enumerate(POSITIONS):
            item = QPushButton(arrows[index])
            item.setObjectName('position')
            bind(item, 'toolTip', title)
            bind(item, 'accessibleName', title)
            item.setCheckable(True)
            item.setFixedHeight(28)
            self.position_group.addButton(item, index)
            grid.addWidget(item, index//3, index%3)
        self.position_group.idClicked.connect(self.changed)
        form.addRow(label('Pozycja'), grid)
        self.spin(form, 'width', 'Szerokość', 220, 1800, ' px')
        self.spin(form, 'font_size', 'Tekst', 8, 64, ' pt')
        self.spin(form, 'lines', 'Wiadomości', 1, 32)
        slider_row = QHBoxLayout()
        self.opacity = QSlider(Qt.Orientation.Horizontal)
        self.opacity.setRange(0, 100)
        self.opacity.valueChanged.connect(self.changed)
        slider_row.addWidget(self.opacity, 1)
        self.opacity_label = label('62%')
        self.opacity_label.setFixedWidth(36)
        slider_row.addWidget(self.opacity_label)
        form.addRow(label('Krycie tła'), slider_row)
        self.check(form, 'show_header', 'Nazwa kanału w nagłówku')
        add_tab(tabs, appearance, 'Wygląd')
        advanced = QWidget()
        adv = QFormLayout(advanced)
        adv.setContentsMargins(0, 2, 0, 0)
        adv.setSpacing(10)
        self.combo(adv, 'text_align', 'Wyrównanie', [('left','Do lewej'), ('center','Do środka'), ('right','Do prawej')])
        self.combo(adv, 'message_order', 'Najnowsze', [('oldest-first','Na dole'), ('newest-first','Na górze')])
        self.spin(adv, 'margin', 'Margines', 0, 500, ' px')
        self.spin(adv, 'header_font_size', 'Nagłówek', 8, 64, ' pt')
        self.spin(adv, 'emote_size', 'Rozmiar emotek', 12, 64, ' px')
        self.spin(adv, 'refresh_ms', 'Odświeżanie', 30, 1000, ' ms')
        self.check(adv, 'emotes', 'Własne emotki YouTube')
        self.combo(adv, 'provider', 'Źródło YouTube', [('auto','Automatycznie'),('yt-dlp','yt-dlp'),('api','YouTube API')])
        api = QLineEdit()
        api.setEchoMode(QLineEdit.EchoMode.Password)
        api.setMaxLength(255)
        bind(api, 'placeholderText', 'Tylko dla YouTube API')
        api.textChanged.connect(self.changed)
        self.fields['youtube_api_key'] = api
        adv.addRow(label('Klucz API'), api)
        advanced_scroll = QScrollArea()
        advanced_scroll.setWidgetResizable(True)
        advanced_scroll.setWidget(advanced)
        add_tab(tabs, advanced_scroll, 'Zaawansowane')
        settings_layout.addWidget(tabs, 1)
        columns.addWidget(settings, 4)
        main.addLayout(columns)
        steam, steam_layout = card()
        steam_head = QHBoxLayout()
        steam_head.addWidget(label('03  Opcje uruchamiania Steam', 'section'), 1)
        self.mangohud = checkbox('Dodaj MangoHud')
        self.mangohud.toggled.connect(self.changed)
        steam_head.addWidget(self.mangohud)
        steam_layout.addLayout(steam_head)
        self.command = QTextEdit()
        self.command.setReadOnly(True)
        self.command.setFixedHeight(66)
        self.command.setFont(QFont('monospace', 10))
        steam_layout.addWidget(self.command)
        command_row = QHBoxLayout()
        guide = label('Steam → gra → Właściwości → Ogólne → Opcje uruchamiania', 'muted')
        guide.setWordWrap(True)
        command_row.addWidget(guide, 1)
        command_row.addWidget(button('Kopiuj komendę', self.copy_command, 'primary'))
        steam_layout.addLayout(command_row)
        steam_layout.addWidget(label('Jeśli masz już własne opcje uruchamiania, połącz je ręcznie przed wklejeniem.', 'muted'))
        main.addWidget(steam)
        bottom = QHBoxLayout()
        self.status = label('', 'muted')
        self.status.setWordWrap(True)
        bottom.addWidget(self.status, 1)
        bottom.addWidget(button('Diagnostyka', self.diagnostics, 'quiet'))
        bottom.addWidget(button('Folder profili', self.open_profiles, 'quiet'))
        main.addLayout(bottom)
        area.setWidget(content)
        outer.addWidget(area, 1)
        self.load_profile()
        QTimer.singleShot(0, self.refresh_monitors)

    def change_language(self):
        selected = self.language_switch.currentData()
        if selected == self.language:
            return
        try:
            save_language(selected)
        except (OSError, ValueError) as error:
            self.language_switch.blockSignals(True)
            self.language_switch.setCurrentIndex(self.language_switch.findData(self.language))
            self.language_switch.blockSignals(False)
            self.error(error)
            return
        self.language = selected
        set_language(selected)
        self.retranslate()
        self.changed()

    def retranslate(self):
        for widget in [self, *self.findChildren(QWidget)]:
            for kind in ('text', 'windowTitle', 'toolTip', 'placeholderText', 'accessibleName'):
                original = widget.property('i18n_' + kind)
                if original is None:
                    continue
                if widget is self.heading and self.current['id'] != 'default':
                    continue
                getattr(widget, 'set' + kind[0].upper() + kind[1:])(tr(original))
            if isinstance(widget, QComboBox):
                previous = widget.blockSignals(True)
                for index in range(widget.count()):
                    original = widget.itemData(index, int(Qt.ItemDataRole.UserRole)+1)
                    if original is not None:
                        widget.setItemText(index, tr(original))
                widget.blockSignals(previous)
            elif isinstance(widget, QTabWidget):
                for index in range(widget.count()):
                    original = widget.widget(index).property('i18n_tab')
                    if original:
                        widget.setTabText(index, tr(original))
        for index, profile in enumerate(self.store.profiles):
            if profile['id'] == 'default':
                self.profiles.item(index).setText(tr('Profil domyślny'))
        if self.current['id'] == 'default':
            self.heading.setText(tr('Twój czat w grze'))
        self.render()

    def spin(self, form, key, title, minimum, maximum, suffix=''):
        item = QSpinBox()
        item.setRange(minimum, maximum)
        item.setFixedHeight(32)
        item.setSuffix(suffix)
        item.setButtonSymbols(QSpinBox.ButtonSymbols.UpDownArrows)
        item.valueChanged.connect(self.changed)
        self.fields[key] = item
        form.addRow(label(title), item)

    def combo(self, form, key, title, options):
        item = QComboBox()
        item.setFixedHeight(32)
        for value, name in options:
            add_combo_item(item, name, value)
        item.currentIndexChanged.connect(self.changed)
        self.fields[key] = item
        form.addRow(label(title), item)

    def check(self, form, key, title):
        item = checkbox(title)
        item.toggled.connect(self.changed)
        self.fields[key] = item
        form.addRow(item)

    def binary(self, name):
        local = ROOT / name
        if local.is_file() and os.access(local, os.X_OK):
            return local
        found = shutil.which(name)
        return Path(found) if found else Path.home() / '.local/bin' / name

    def refresh_monitors(self):
        if self.monitor_query is not None:
            return
        process = QProcess(self)
        self.monitor_query = process
        process.setProgram(str(self.binary('chathud')))
        process.setArguments(['--list-monitors'])
        environment = QProcessEnvironment.systemEnvironment()
        environment.remove('LD_LIBRARY_PATH')
        environment.remove('LD_PRELOAD')
        process.setProcessEnvironment(environment)
        process.finished.connect(lambda code, status: self.monitors_finished(process, code))
        process.errorOccurred.connect(lambda error: self.monitors_finished(process, -1)
            if error == QProcess.ProcessError.FailedToStart else None)
        QTimer.singleShot(2500, lambda: process.kill() if self.monitor_query is process else None)
        process.start()

    def monitors_finished(self, process, code):
        if self.monitor_query is not process:
            return
        self.monitor_query = None
        output = bytes(process.readAllStandardOutput()).decode('utf-8', errors='replace')
        process.deleteLater()
        try:
            if code != 0:
                raise ValueError('Nie można odczytać ekranów X11/XWayland.')
            monitors = json.loads(output)
            if not isinstance(monitors, list) or not monitors:
                raise ValueError('Brak aktywnych ekranów X11/XWayland.')
            entries = []
            for monitor in monitors:
                name = monitor['name']
                width, height = int(monitor['width']), int(monitor['height'])
                if not isinstance(name, str) or width < 1 or height < 1:
                    raise ValueError('Nieprawidłowe dane monitora.')
                title = f'{name} — {width}×{height}'
                if monitor.get('primary'):
                    title += ' (główny)'
                entries.append((title, name))
        except (ValueError, KeyError, TypeError) as error:
            bind(self.monitor, 'toolTip', str(error) + ' Zapisany wybór zostaje zachowany.')
            return
        selected = self.monitor.currentData() or 'primary'
        self.monitor.blockSignals(True)
        self.monitor.clear()
        add_combo_item(self.monitor, 'Główny monitor', 'primary')
        for title, name in entries:
            add_combo_item(self.monitor, title, name)
        if self.monitor.findData(selected) < 0:
            add_combo_item(self.monitor, selected + ' (odłączony — użyty będzie główny)', selected)
        self.monitor.setCurrentIndex(max(0, self.monitor.findData(selected)))
        self.monitor.blockSignals(False)
        bind(self.monitor, 'toolTip', 'Pozycja czatu jest liczona tylko w obrębie tego monitora. Odłączony ekran zastępuje monitor główny.')

    def values(self):
        values = {'stream_url': self.url.text().strip(),
                  'background_opacity': f'{self.opacity.value()/100:.2f}',
                  'position': POSITIONS[max(0,self.position_group.checkedId())][0],
                  'language': self.language}
        for key, item in self.fields.items():
            if isinstance(item, QSpinBox):
                values[key] = str(item.value())
            elif isinstance(item, QCheckBox):
                values[key] = '1' if item.isChecked() else '0'
            elif isinstance(item, QComboBox):
                values[key] = item.currentData()
            else:
                values[key] = item.text().strip()
        return values

    def load_profile(self):
        self.loading = True
        values = read_config(self.store.path(self.current))
        self.heading.setText(tr('Twój czat w grze') if self.current['id'] == 'default' else self.current['name'])
        self.url.setText(values['stream_url'])
        def number(key, fallback):
            try:
                return int(float(values[key]))
            except (ValueError, OverflowError):
                return fallback
        self.opacity.setValue(self.opacity_value(values['background_opacity']))
        positions = [p[0] for p in POSITIONS]
        self.position_group.button(positions.index(values['position']) if values['position'] in positions else 2).setChecked(True)
        for key, item in self.fields.items():
            if isinstance(item, QSpinBox):
                item.setValue(number(key, int(DEFAULTS[key])))
            elif isinstance(item, QCheckBox):
                item.setChecked(values[key].lower() in ('1','true','yes','on'))
            elif isinstance(item, QComboBox):
                if key == 'monitor' and item.findData(values[key]) < 0:
                    add_combo_item(item, values[key] + ' (zapisany monitor)', values[key])
                item.setCurrentIndex(max(0, item.findData(values[key])))
            else:
                item.setText(values[key])
        self.mangohud.setChecked(bool(self.current.get('mangohud', False)))
        self.loading = False
        self.dirty = False
        self.render()
        bind(self.save_button, "text", 'Zapisz profil')
        bind(self.status, "text", 'Gotowe do konfiguracji. ' + ('Brak yt-dlp — szczegóły w Diagnostyce.' if not shutil.which('yt-dlp') else 'yt-dlp jest dostępny.'))

    @staticmethod
    def opacity_value(text):
        try:
            return round(float(text)*100)
        except (ValueError, OverflowError):
            return 62

    def render(self):
        self.preview.values = self.values()
        self.preview.update()
        self.opacity_label.setText(f'{self.opacity.value()}%')
        self.command.setPlainText(launch_options(self.binary('chathud-steam'), self.store.path(self.current), self.mangohud.isChecked()))

    def changed(self, *args):
        if self.loading:
            return
        self.dirty = True
        bind(self.save_button, "text", 'Zapisz zmiany •')
        bind(self.status, "text", 'Niezapisane zmiany. Komenda zostanie zapisana przed skopiowaniem.')
        self.render()

    def error(self, message):
        QMessageBox.warning(self, 'ChatHUD', tr(str(message)))

    def save(self):
        try:
            values = self.values()
            values['stream_url'] = validate_url(values['stream_url'])
            write_config(self.store.path(self.current), values)
            self.current['mangohud'] = self.mangohud.isChecked()
            self.store.save()
        except (OSError, ValueError) as error:
            self.error(error)
            return False
        self.loading = True
        self.url.setText(values['stream_url'])
        self.loading = False
        self.dirty = False
        bind(self.save_button, "text", 'Zapisz profil')
        bind(self.status, "text", 'Profil zapisany. Zmiany zadziałają przy następnym uruchomieniu overlaya.')
        return True

    def leave_profile(self):
        if not self.dirty:
            return True
        question = QMessageBox(self)
        bind(question, 'windowTitle', tr('Niezapisane zmiany'))
        question.setText(tr('Zapisać zmiany w bieżącym profilu?'))
        question.setStandardButtons(QMessageBox.StandardButton.Save | QMessageBox.StandardButton.Discard | QMessageBox.StandardButton.Cancel)
        question.setDefaultButton(QMessageBox.StandardButton.Save)
        for key, text in [(QMessageBox.StandardButton.Save, 'Zapisz'), (QMessageBox.StandardButton.Discard, 'Odrzuć'), (QMessageBox.StandardButton.Cancel, 'Anuluj')]:
            question.button(key).setText(tr(text))
        answer = question.exec()
        if answer == QMessageBox.StandardButton.Save:
            return self.save()
        return answer == QMessageBox.StandardButton.Discard

    def switch_profile(self, index):
        if index < 0 or self.loading:
            return
        if not self.leave_profile():
            self.profiles.blockSignals(True)
            self.profiles.setCurrentRow(self.store.profiles.index(self.current))
            self.profiles.blockSignals(False)
            return
        old = self.current
        self.current = self.store.profiles[index]
        try:
            self.load_profile()
        except (OSError, ValueError) as error:
            self.current = old
            self.profiles.blockSignals(True)
            self.profiles.setCurrentRow(self.store.profiles.index(old))
            self.profiles.blockSignals(False)
            self.loading = False
            self.error(error)

    def add_profile(self):
        if not self.leave_profile():
            return
        dialog = QDialog(self)
        bind(dialog, 'windowTitle', 'Nowy profil gry')
        dialog.setMinimumWidth(420)
        layout = QVBoxLayout(dialog)
        layout.addWidget(label('Wybierz grę Steam lub wpisz własną nazwę.', 'section'))
        names = QComboBox()
        names.setEditable(True)
        names.addItems(steam_games())
        names.setCurrentIndex(-1)
        bind(names, 'placeholderText', 'Nazwa gry / profilu')
        layout.addWidget(names)
        layout.addWidget(label('Każdy profil ma własny link i wygląd czatu.', 'muted'))
        actions = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        actions.button(QDialogButtonBox.StandardButton.Cancel).setText(tr('Anuluj'))
        actions.accepted.connect(dialog.accept)
        actions.rejected.connect(dialog.reject)
        layout.addWidget(actions)
        if dialog.exec() != QDialog.DialogCode.Accepted:
            return
        try:
            self.store.add(names.currentText(), read_config(self.store.path(self.current)))
        except (OSError, ValueError) as error:
            self.error(error)
            return
        self.loading = True
        self.profiles.addItem(self.store.profiles[-1]['name'])
        self.profiles.setCurrentRow(len(self.store.profiles)-1)
        self.current = self.store.profiles[-1]
        self.load_profile()

    def copy_command(self):
        if not self.url.text().strip():
            self.error('Najpierw wklej link do YouTube Live lub Twitch. Do sprawdzenia wyglądu użyj testu na pulpicie.')
            return
        if not os.access(self.binary('chathud-steam'), os.X_OK) or not os.access(self.binary('chathud'), os.X_OK):
            self.error('Brakuje overlaya lub launchera. W katalogu projektu uruchom ./install.sh.')
            return
        if self.mangohud.isChecked() and not shutil.which('mangohud'):
            self.error('MangoHud nie jest dostępny. Zainstaluj go lub wyłącz opcję „Dodaj MangoHud”.')
            return
        if self.save():
            QApplication.clipboard().setText(self.command.toPlainText())
            bind(self.status, "text", 'Skopiowano. Wklej komendę w opcjach uruchamiania wybranej gry w Steam.')

    def start_overlay(self, demo):
        if self.process is not None:
            return
        binary = self.binary('chathud')
        if not os.access(binary, os.X_OK):
            self.error('Brak programu chathud. W katalogu projektu uruchom make lub ./install.sh.')
            return
        if not os.environ.get('DISPLAY'):
            self.error('Overlay wymaga X11 lub XWayland (DISPLAY). Panel ustawień działa także bez niego.')
            return
        if not demo:
            if not self.url.text().strip():
                self.error('Wklej link do YouTube Live lub Twitch.')
                return
            provider = self.fields['provider'].currentData()
            if stream_platform(self.url.text()) == 'youtube' and provider in ('yt-dlp','auto') and not shutil.which('yt-dlp'):
                if provider == 'yt-dlp' or not self.fields['youtube_api_key'].text().strip():
                    self.error('Brak yt-dlp. Arch: sudo pacman -S yt-dlp\nUbuntu / Debian: sudo apt install yt-dlp\nMożesz też wybrać YouTube API i podać klucz.')
                    return
        try:
            values = self.values()
            if not demo:
                values['stream_url'] = validate_url(values['stream_url'])
            # Separate snapshot: demo never overwrites a user's profile.
            import tempfile
            directory = Path(tempfile.mkdtemp(prefix='chathud-preview-'))
            self.preview_file = directory / 'config'
            write_config(self.preview_file, values)
        except (OSError, ValueError) as error:
            self.cleanup_preview()
            self.error(error)
            return
        process = QProcess(self)
        self.process = process
        self.stopping = False
        self.process_output = ''
        process.setProgram(str(binary))
        arguments = ['--config', str(self.preview_file), '--worker', '--watch-pid', str(os.getpid())]
        if demo:
            arguments.append('--demo')
        process.setArguments(arguments)
        environment = QProcessEnvironment.systemEnvironment()
        environment.remove('LD_LIBRARY_PATH')
        environment.remove('LD_PRELOAD')
        process.setProcessEnvironment(environment)
        process.setProcessChannelMode(QProcess.ProcessChannelMode.MergedChannels)
        process.readyReadStandardOutput.connect(self.read_process_output)
        process.finished.connect(self.overlay_finished)
        process.errorOccurred.connect(self.process_error)
        self.demo_button.setEnabled(False)
        self.live_button.setEnabled(False)
        self.stop_button.setEnabled(True)
        bind(self.status, "text", 'Test na pulpicie działa. Użyj przycisku ■, aby zatrzymać overlay.')
        process.start()

    def read_process_output(self):
        if self.process:
            self.process_output = (self.process_output + bytes(self.process.readAllStandardOutput()).decode('utf-8', errors='replace'))[-8000:]

    def process_error(self, error):
        if error == QProcess.ProcessError.FailedToStart:
            message = self.process.errorString()
            self.overlay_finished(-1, QProcess.ExitStatus.CrashExit)
            self.error(message)

    def cleanup_preview(self):
        if self.preview_file:
            self.preview_file.unlink(missing_ok=True)
            self.preview_file.parent.rmdir()
            self.preview_file = None

    def overlay_finished(self, code, status):
        self.read_process_output()
        output = self.process_output
        if self.process:
            self.process.deleteLater()
        self.process = None
        self.cleanup_preview()
        self.demo_button.setEnabled(True)
        self.live_button.setEnabled(True)
        self.stop_button.setEnabled(False)
        failed = code != 0 and not self.stopping
        bind(self.status, "text", 'Test zakończony.' if not failed else f'Overlay zakończył się z błędem ({code}).')
        if failed and output:
            self.error(output[-1800:])

    def stop_overlay(self):
        if self.process:
            self.stopping = True
            process = self.process
            process.terminate()
            QTimer.singleShot(2500, lambda: process.kill() if self.process is process and process.state() != QProcess.ProcessState.NotRunning else None)

    def diagnostics(self):
        lines = [f'ChatHUD {VERSION}', '']
        for name in ('chathud', 'chathud-steam'):
            path = self.binary(name)
            lines.append(f'{name}: {"OK" if os.access(path, os.X_OK) else tr("BRAK")}\n{path}')
        lines.extend(['', f'yt-dlp: {shutil.which("yt-dlp") or "BRAK — sudo pacman -S yt-dlp (Arch) / sudo apt install yt-dlp (Debian/Ubuntu)"}',
                      f'MangoHud: {shutil.which("mangohud") or "niewykryty (opcjonalny)"}',
                      f'Sesja: {os.environ.get("XDG_SESSION_TYPE") or tr("nieznana")}',
                      f'DISPLAY: {os.environ.get("DISPLAY", "BRAK — overlay wymaga X11/XWayland")}',
                      '', 'Overlay używa X11/XWayland. Widoczność nad grami Wayland zależy od kompozytora.',
                      f'Profil: {self.store.path(self.current)}',
                      f'Log Steam: {Path(os.environ.get("XDG_CACHE_HOME") or Path.home()/".cache") / "chathud/steam-wrapper.log"}'])
        dialog = QMessageBox(self)
        bind(dialog, 'windowTitle', 'ChatHUD — diagnostyka')
        dialog.setTextFormat(Qt.TextFormat.PlainText)
        bind(dialog, "text", '\n'.join(lines))
        dialog.exec()

    def open_profiles(self):
        try:
            self.store.root.mkdir(parents=True, exist_ok=True)
            QDesktopServices.openUrl(QUrl.fromLocalFile(str(self.store.root)))
        except OSError as error:
            self.error(error)

    def closeEvent(self, event):
        if not self.leave_profile():
            event.ignore()
            return
        if self.monitor_query:
            query = self.monitor_query
            query.kill()
            query.waitForFinished(500)
        if self.process:
            self.stopping = True
            self.process.terminate()
            if not self.process.waitForFinished(2000):
                self.process.kill()
                self.process.waitForFinished(1000)
        self.cleanup_preview()
        event.accept()


def main():
    app = QApplication(['ChatHUD', *sys.argv[1:]])
    app.setApplicationName('ChatHUD')
    app.setOrganizationDomain('devvoidpl.github.io')
    app.setDesktopFileName(APP_ID)
    app.setWindowIcon(QIcon(str(ROOT / 'assets/chathud.png')))
    app.setStyle('Fusion')
    app.setStyleSheet(STYLE.replace('CHECK_ICON', str(ROOT / 'assets/check.svg')))
    try:
        window = Window()
    except (OSError, ValueError, KeyError, TypeError) as error:
        QMessageBox.critical(None, tr('ChatHUD — błąd konfiguracji'), tr(f'Nie można odczytać profili. Pliki pozostawiono bez zmian.\n\n{error}'))
        return 1
    window.show()
    return app.exec()


if __name__ == '__main__':
    sys.exit(main())
