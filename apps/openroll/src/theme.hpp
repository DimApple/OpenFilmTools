#pragma once

#include <QApplication>
#include <QFont>
#include <QHash>
#include <algorithm>
#include <QLabel>
#include <QPalette>
#include <QStyle>

namespace OpenRollTheme {
inline const QHash<QString, QString> tokens{
    {"@bg", "#111113"},
    {"@surface", "#18181B"},
    {"@raised", "#222227"},
    {"@hover", "#2B2B31"},
    {"@line", "#2C2C32"},
    {"@text", "#E8E8EC"},
    {"@muted", "#8D8D96"},
    {"@disabled", "#5A5A63"},
    {"@accent", "#AE75E9"},
    {"@accentHover", "#C08DF2"},
    {"@accentSoft", "#2E2340"},
    {"@onAccent", "#1B1322"},
    {"@success", "#8FD0AD"},
    {"@warning", "#E3BD8A"},
    {"@danger", "#E58A8A"},
    {"@rControl", "6px"},
    {"@rCard", "10px"},
    {"@rContainer", "14px"},
    {"@scroll", "#34343B"},
    {"@scrollHover", "#44444C"},
    {"@white", "#FFFFFF"},
    {"@rScrollbar", "3px"},
    {"@rTrack", "2px"},
};
}

inline QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("sectionLabel");
    return label;
}

inline void setAction(QWidget* widget, const char* role)
{
    widget->setProperty("actionRole", QString::fromLatin1(role));
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

inline void applyTheme(QApplication& app)
{
    app.setStyle("Fusion");
    auto font = app.font();
    font.setPixelSize(13);
    app.setFont(font);

    const auto& tokens = OpenRollTheme::tokens;
    const auto color = [&](const char* key) { return QColor(tokens.value(QString::fromLatin1(key))); };
    QPalette palette;
    palette.setColor(QPalette::Window, color("@bg"));
    palette.setColor(QPalette::Base, color("@surface"));
    palette.setColor(QPalette::AlternateBase, color("@raised"));
    palette.setColor(QPalette::Button, color("@raised"));
    palette.setColor(QPalette::Text, color("@text"));
    palette.setColor(QPalette::WindowText, color("@text"));
    palette.setColor(QPalette::ButtonText, color("@text"));
    palette.setColor(QPalette::PlaceholderText, color("@muted"));
    palette.setColor(QPalette::Highlight, color("@accentSoft"));
    palette.setColor(QPalette::HighlightedText, color("@text"));
    palette.setColor(QPalette::ToolTipBase, color("@raised"));
    palette.setColor(QPalette::ToolTipText, color("@text"));
    for (const auto role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText})
        palette.setColor(QPalette::Disabled, role, color("@disabled"));
    app.setPalette(palette);

    QString stylesheet = QString::fromLatin1(R"QSS(
QDialog { background: @bg; }
QMainWindow, QWidget#appRoot { background: @bg; }

QLabel#sectionLabel { color: @text; font-size: 15px; font-weight: 600; }
QLabel#quiet { color: @muted; font-size: 11px; }
QLabel[actionRole="success"] { color: @success; }
QLabel[actionRole="warning"] { color: @warning; }
QLabel[actionRole="danger"]  { color: @danger; }

/* Cards and panels: tone only, no border */
QFrame#card, QFrame#mediaPanel { background: @surface; border: 0; border-radius: @rCard; }

/* Buttons */
QPushButton {
  background: @raised; color: @text; border: 1px solid transparent;
  border-radius: @rControl; padding: 0 12px; min-height: 32px;
}
QPushButton:hover { background: @hover; }
QPushButton:pressed { background: @surface; }
QPushButton:focus { border-color: @accent; }
QPushButton:disabled { background: @surface; color: @disabled; }
QPushButton[actionRole="primary"] { background: @accent; color: @onAccent; font-weight: 600; }
QPushButton[actionRole="primary"]:hover { background: @accentHover; }
QPushButton[actionRole="primary"]:disabled { background: @raised; color: @disabled; }
QPushButton[actionRole="ghost"] { background: transparent; }
QPushButton[actionRole="ghost"]:hover { background: @raised; }
QPushButton[actionRole="link"] { background: transparent; color: @muted; padding: 0 4px; min-height: 24px; }
QPushButton[actionRole="link"]:hover { color: @text; }

/* Inputs */
QLineEdit, QComboBox, QSpinBox {
  background: @raised; border: 1px solid transparent; border-radius: @rControl;
  padding: 0 8px; min-height: 32px; selection-background-color: @accentSoft;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus { border-color: @accent; }
/* Leave QComboBox::drop-down and ::down-arrow UNSTYLED so Fusion keeps drawing the arrow. */
QComboBox QAbstractItemView {
  background: @surface; border: 1px solid @line; outline: 0;
  selection-background-color: @accentSoft; selection-color: @text;
}

/* Lists and trees */
QListView, QTreeView, QListWidget, QTreeWidget { background: transparent; border: 1px solid transparent; outline: 0; }
QListView:focus, QTreeView:focus { border-color: @accent; }
QTreeView::item, QListView::item { padding: 4px 8px; border-radius: @rControl; }
QTreeView::item:hover, QListView::item:hover { background: @raised; }
QTreeView::item:selected, QListView::item:selected { background: @accentSoft; color: @text; }
QHeaderView::section { background: transparent; color: @muted; border: 0; padding: 4px 8px; font-size: 11px; }

/* Scrollbars: slim, rounded, no arrows */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: @scroll; border-radius: @rScrollbar; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: @scrollHover; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: @scroll; border-radius: @rScrollbar; min-width: 32px; }
QScrollBar::handle:horizontal:hover { background: @scrollHover; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* Sliders (scrubber and volume) */
QSlider::groove:horizontal { height: 4px; background: @raised; border-radius: @rTrack; }
QSlider::sub-page:horizontal { background: @muted; border-radius: @rTrack; }
QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: @rControl; background: @text; }
QSlider::handle:horizontal:hover { background: @white; }

/* Progress: slim, no text inside */
QProgressBar { background: @raised; border: 0; border-radius: @rScrollbar; max-height: 6px; min-height: 6px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @muted; border-radius: @rScrollbar; }

/* Menus: leave popup corners square, do not attempt translucent frameless menus */
QMenu { background: @surface; border: 1px solid @line; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: @rControl; }
QMenu::item:selected { background: @accentSoft; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }

QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 8px; }
QSplitter::handle:vertical { height: 8px; }

QToolTip { background: @raised; color: @text; border: 1px solid @line; padding: 4px 8px; }


QLabel#welcomeName { font-size: 22px; font-weight: 600; }
QWidget#recentRow { background: @surface; border-radius: @rCard; }
QListWidget#recentList::item:hover { background: @raised; border-radius: @rCard; }
QLabel#recentEmpty { border: 1px dashed @line; border-radius: @rCard; color: @muted; }
QWidget#appHeader { background: @surface; }
QLabel#appName { color: @text; font-size: 15px; font-weight: 600; }
QLabel#appProject { color: @muted; font-size: 11px; }
QLabel#ingestStep {
    background: @raised; color: @muted; border-radius: 10px; font-size: 11px; font-weight: 600;
}
QTreeWidget#ingestDestinations::item {
    background: @raised; min-height: 40px; border-radius: @rCard;
}
QTreeWidget#ingestDestinations::item:selected { background: @accentSoft; }
QLabel#sourceDrop {
    background: transparent; border: 1px dashed @line; border-radius: @rContainer;
    color: @text; padding: 12px;
}
QWidget#workspaceSwitch { background: @raised; border-radius: 18px; }
QToolButton#workspaceButton {
    background: transparent; border: 0; border-radius: 15px;
    color: @muted; padding: 0 14px; font-weight: 600;
}
QToolButton#workspaceButton:hover { color: @text; }
QToolButton#workspaceButton:checked { background: @accentSoft; color: @text; }
QWidget#destinationActions[actionRole="empty"] { border: 1px dashed @line; border-radius: @rCard; }
QLabel#sourceDrop[dragActive="true"] { border: 1px solid @accent; background: @accentSoft; }
QToolButton#projectMenu, QToolButton#viewToggle {
    background: @raised; border: 1px solid transparent; border-radius: @rControl;
    padding: 4px 12px;
}
QToolButton#projectMenu:hover, QToolButton#viewToggle:hover { background: @hover; }
QToolButton#viewToggle:checked { background: @accentSoft; }
QToolButton#projectMenu { padding-right: 24px; }
QToolButton#projectMenu[emptyProject="true"] { color: @muted; }
QToolButton#projectMenu:focus, QToolButton#viewToggle:focus { border-color: @accent; }
QTreeWidget#mediaFolders, QTreeWidget#metadataList, QListWidget#clipStrip { background: transparent; }
QPushButton#transportButton { padding: 0; min-height: 0; border-radius: 16px; }
QPushButton#transportButton[actionRole="primary"] { border-radius: 18px; }
QPushButton#addMedia { padding: 0; min-height: 0; }
QToolButton#metadataToggle { border: 1px solid transparent; border-radius: @rControl; }
QToolButton#metadataToggle:checked { background: @accentSoft; }
QToolButton#metadataToggle:focus { border-color: @accent; }
QLabel#quiet[monitor="true"] { font-size: 14px; }
QLabel#metadataValue { font-size: 12px; }
QTreeWidget#mediaFolders::item { min-height: 20px; }
QWidget#clipViews { background: @raised; border-radius: 14px; }
QToolButton#viewToggle { padding: 0; border-radius: 11px; }
QPushButton#transportButton:focus { border-color: @accent; }
QTextEdit#exportResults { background: @bg; border: 0; border-radius: @rCard; font-size: 12px; }
QPlainTextEdit, QTextEdit {
    background: @surface; border: 1px solid transparent; border-radius: @rControl;
}
QPlainTextEdit:focus, QTextEdit:focus { border-color: @accent; }
)QSS");
    auto keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
        return a.size() > b.size();
    });
    for (const auto& key : keys) stylesheet.replace(key, tokens.value(key));
    app.setStyleSheet(stylesheet);
}
