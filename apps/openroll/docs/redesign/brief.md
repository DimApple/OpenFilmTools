# OpenRoll UI redesign brief

You are implementing a visual and layout redesign of OpenRoll 0.14.0, a Qt 6 Widgets desktop app (C++17, Meson, styled with QSS on top of the Fusion style). I am the product owner and a working cinematographer. This brief is exact on purpose. Follow it literally. If something here conflicts with the code, stop and tell me instead of improvising.

## Goal

Minimalist, rounded, modern, very easy to use, and still powerful. Minimalist means fewer things visible at once, not fewer features. Power comes from keyboard shortcuts, information that appears when needed, and screens that always show the next step.

## Hard constraints

1. Qt Widgets and QSS only. No QML, no new dependencies, no new `.qrc` resource file (`meson.build` has none and I do not want one).
2. Do not touch engine code. These files must not change logic: `ingest.cpp`, `proxy.cpp`, `project.cpp`, `media_scan.cpp`, `media.cpp` data and scanning logic, `raw.cpp`, `vlc.cpp`, `playback.cpp` playback logic. UI construction code inside the widget files is fair game.
3. Keep every public class API in the `.hpp` files unchanged unless a step below says otherwise. `tests/new_project.cpp` and `tests/transcode_project.cpp` compile `src/new_project.cpp` and `src/proxy_widget.cpp` directly. Before renaming any `objectName` or visible button text, grep `tests/` for it.
4. After every step, `meson compile -C build` and `meson test -C build --print-errorlogs` must pass. The smoke test (`--smoke-test`) clicks the four workspace buttons and expects them to be checkable and checked in turn. Keep that working.
5. No drop shadows (`QGraphicsDropShadowEffect`), no animations, no gradients, no emoji. Surfaces are separated by tone, not by borders.
6. Do not round or restyle the native window frame. Native KDE title bar stays.
7. Do not rewrite whole files to restyle. Change only what each step lists.

## Process

Work one step at a time, in the order below. After each step, give me the changed files in full (or a unified diff), then stop and wait. I will test it, send a screenshot, and tell you to continue. Do not start the next step on your own.

---

## Step 0: Baseline

I will commit or copy the current tree before you start. Nothing for you to do except confirm the build and tests pass before changes.

---

## Step 1: Design tokens and global theme (`src/theme.hpp` only)

No layout changes in this step. Every screen should simply look neutral, calmer and rounded afterwards.

### 1.1 Token system

Put all values in one place at the top of `theme.hpp`. Write the stylesheet using `@token` placeholders and substitute them once in `applyTheme()` (for example with a `QHash<QString, QString>` and a replace loop sorted longest-name-first so `@accentSoft` is not clobbered by `@accent`). No hex color or radius is allowed to appear anywhere else in the codebase after the redesign.

**Colors (neutral gray, no purple or blue cast)**

| Token | Value | Use |
|---|---|---|
| `@bg` | `#111113` | window background, deepest layer |
| `@surface` | `#18181B` | panels, cards, header |
| `@raised` | `#222227` | buttons, inputs, list hover base |
| `@hover` | `#2B2B31` | hover state on raised things |
| `@line` | `#2C2C32` | the one hairline, used sparingly |
| `@text` | `#E8E8EC` | primary text |
| `@muted` | `#8D8D96` | labels, metadata, secondary text |
| `@disabled` | `#5A5A63` | disabled text |
| `@accent` | `#AE75E9` | primary action fill, current selection indicator |
| `@accentHover` | `#C08DF2` | primary hover |
| `@accentSoft` | `#2E2340` | selected row/segment background |
| `@onAccent` | `#1B1322` | text/icons on accent fill |
| `@success` | `#8FD0AD` | success text |
| `@warning` | `#E3BD8A` | warning text |
| `@danger` | `#E58A8A` | error text |

**Accent rule.** Accent means exactly two things: the one primary action on the current screen, and the current selection. Nothing else is purple. The dashed drop zone, progress chunks, section titles, logo-adjacent decoration and focus rings must not use accent fill (a 1px accent focus border on inputs and buttons is allowed).

**Radii**

| Token | Value | Use |
|---|---|---|
| `@rControl` | `6px` | buttons, inputs, combo boxes, small icon buttons |
| `@rCard` | `10px` | panels, cards, list rows, clip cards, recent-project rows |
| `@rContainer` | `14px` | the Ingest drop zone only |
| pill | height / 2 | the workspace switcher and the Media view toggle only |

Each radius is used for its listed role only. Do not make everything a pill.

**Spacing.** Only these values exist: `4, 8, 12, 16, 24` px. Margins, paddings and layout spacing in every widget use only these. Default layout spacing is 8, card padding is 16, window-edge margin is 12.

**Type.** Keep the system font. Three sizes only:

| Name | Size | Weight | Use |
|---|---|---|---|
| title | 15px | 600 | app name, card/section titles |
| body | 13px | 400 | everything default |
| small | 11px | 400 | metadata, hints, counts, timecode labels |

Body text uses `@text`. Labels and hints use `@muted`. Set the application font so body is 13px, then size titles and small text through QSS.

### 1.2 Palette

Update the `QPalette` in `applyTheme()` to the new neutral values so unstyled Fusion parts (menus, popups, tooltips, dialogs) match: `Window=@bg`, `Base=@surface`, `AlternateBase=@raised`, `Button=@raised`, `Text/WindowText/ButtonText=@text`, `PlaceholderText=@muted`, `Highlight=@accentSoft`, `HighlightedText=@text`, `ToolTipBase=@raised`, `ToolTipText=@text`, disabled text `@disabled`.

### 1.3 Reference QSS (adapt, keep the intent)

```css
QWidget { background: transparent; color: @text; font-size: 13px; }
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
QLineEdit:focus, QComboBox:focus { border-color: @accent; }
/* Leave QComboBox::drop-down and ::down-arrow UNSTYLED so Fusion keeps drawing the arrow. */
QComboBox QAbstractItemView {
  background: @surface; border: 1px solid @line; outline: 0;
  selection-background-color: @accentSoft; selection-color: @text;
}

/* Lists and trees */
QListView, QTreeView, QListWidget, QTreeWidget { background: transparent; border: 0; outline: 0; }
QTreeView::item, QListView::item { padding: 4px 8px; border-radius: @rControl; }
QTreeView::item:hover, QListView::item:hover { background: @raised; }
QTreeView::item:selected, QListView::item:selected { background: @accentSoft; color: @text; }
QHeaderView::section { background: transparent; color: @muted; border: 0; padding: 4px 8px; font-size: 11px; }

/* Scrollbars: slim, rounded, no arrows */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #34343B; border-radius: 3px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: #44444C; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
/* same for horizontal */

/* Sliders (scrubber and volume) */
QSlider::groove:horizontal { height: 4px; background: @raised; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
QSlider::handle:horizontal { width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; background: @text; }
QSlider::handle:horizontal:hover { background: #FFFFFF; }

/* Progress: slim, no text inside */
QProgressBar { background: @raised; border: 0; border-radius: 3px; max-height: 6px; min-height: 6px; text-align: center; color: transparent; }
QProgressBar::chunk { background: @accent; border-radius: 3px; }

/* Menus: leave popup corners square, do not attempt translucent frameless menus */
QMenu { background: @surface; border: 1px solid @line; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 12px; border-radius: @rControl; }
QMenu::item:selected { background: @accentSoft; }
QMenu::separator { height: 1px; background: @line; margin: 4px 8px; }

QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 8px; }
QSplitter::handle:vertical { height: 8px; }

QToolTip { background: @raised; color: @text; border: 1px solid @line; padding: 4px 8px; }
```

Remove every old rule that sets a border on a section container: `QGroupBox#ingestSection` and its `::title`, the old `QFrame#mediaPanel` border, the old `QLabel#sourceDrop` fill. Keep all existing `objectName`s working (`appHeader`, `workspaceNav`, `appName`, `appProject`, `sourceDrop`, `sectionLabel`, `quiet`, `workspaceButton`, `mediaPanel`, `mediaFolders`, `metadataList`, `clipStrip`, `projectMenu`, `viewToggle`, `transportButton`, `actionRole` property) so later steps can reshape layouts incrementally. Add `"ghost"`, `"link"` and `"danger"` to the allowed `actionRole` values via the existing `setAction()` helper.

### 1.4 Rounded-corner rules (read this, it will bite you)

- QSS rounds a widget's own background but does not clip its children. A square pixmap or video surface inside a rounded panel pokes out at the corners. Fix by inset (give the panel 6 to 8px padding so content never touches the corners) or by painting through a `QPainterPath` clip in a delegate or `paintEvent`. Never leave a child square against a rounded corner.
- Native video surfaces (VLC / SDK viewer) cannot be clipped reliably. Use the inset approach for the viewer.
- Do not use `setMask` on the video widget.

### Step 1 acceptance

All four screens plus the welcome page build and look neutral gray. No section has a visible border box. Buttons are 32px tall with 6px radius. Scrollbars are slim. Nothing is laid out differently yet.

---

## Step 2: Header, workspace switcher, project menu (`src/main.cpp`)

### 2.1 Decision: move the workspace switcher into the header

Delete the bottom `workspaceNav` bar entirely. This reclaims about 90px of vertical space for the viewer and lists.

New header, fixed height 48px, background `@surface`, no bottom border (the tone change against `@bg` is the edge):

```
[logo 20px] OpenRoll          [ Ingest | Media | Monitor | Transcode ]          [ ProjectName v ]
  left, 16px margin                   centered pill switcher                       right, 16px margin
```

Use a three-column layout: left block, a stretch, the centered switcher, a stretch, the right block. The left and right blocks must have equal fixed widths so the switcher is truly centered (set both to 240px, or use a `QGridLayout` with equal stretch on columns 0 and 2).

### 2.2 Logo and name

Render the logo `QPixmap` at 20x20 logical pixels (paint at 2x with `setDevicePixelRatio(2)` so it stays sharp). App name is `appName`, 15px, weight 600, `@text`.

### 2.3 Workspace switcher (segmented pill)

- A container `QWidget#workspaceSwitch`: background `@raised`, radius 18px, padding 3px, height 36px.
- Four `QToolButton#workspaceButton`, checkable, in the existing `QButtonGroup`, same ids 0 to 3, same labels, same tooltips (`Ingest (Ctrl+1)` etc.), same accessible names, same `QShortcut` Ctrl+1 to Ctrl+4, same `idClicked` behavior. Only the presentation changes.
- Switch `setToolButtonStyle` from `Qt::ToolButtonTextUnderIcon` to `Qt::ToolButtonTextBesideIcon`. Icon size 16x16.
- `setFocusPolicy(Qt::NoFocus)` on all four. This fixes the current bug where the Ingest button keeps an outline on other workspaces and looks selected.
- Button QSS: height 30px, radius 15px, padding `0 14px`, background transparent, text `@muted`, weight 600. Hover: text `@text`. Checked: background `@accentSoft`, text `@text`, no border.
- Icons: change `workspaceIcon(int index)` to take a color and build a `QIcon` with two pixmaps: `QIcon::Normal, QIcon::Off` in `@muted` and `QIcon::Normal, QIcon::On` in `@text`. Keep the existing drawing code, render at 2x device pixel ratio.
- Ingest, Media, Monitor, Transcode order and labels do not change. The smoke test must still pass: the buttons must remain checkable and in the same `QButtonGroup`.

### 2.4 Project menu (right side)

- `QToolButton#projectMenu`, height 32px, radius 6px, background `@raised`, instant-popup menu, `Qt::ToolButtonTextOnly`, elided middle at 200px (existing behavior).
- Text is the project name, or `No project` (muted) when none is open.
- Menu contents, in order:
  1. `New project…` (Ctrl+N)
  2. `Open project…` (Ctrl+O)
  3. `Recent projects` submenu (filled in Step 3; for now disabled)
  4. separator
  5. `Open clip…` (Ctrl+Shift+O). This is the current "Open media…" action moved here.
  6. separator
  7. `Close project` (disabled when none is open)
- Register the shortcuts on the main window so they work from every workspace.
- Remove the `appProject` label and the "Project: LeviTest" label on Ingest. The project name appears once, in this button.

### Step 2 acceptance

One 48px header with logo, centered pill switcher, project button. No bottom bar. Ctrl+1 to 4, Ctrl+N, Ctrl+O, Ctrl+Shift+O all work. No stuck outline on any switcher button. Smoke test passes.

---

## Step 3: Welcome page (the "Create or open a project" screen)

Find the widget that renders "Create or open a project" and rebuild its layout. Keep the existing project create/open/continue code paths and signals.

### 3.1 Layout

Single centered column, max width 480px, horizontally centered, vertically biased slightly above center (use stretch 2 above and 3 below). Margins 24.

1. Logo at 40px, app name 22px weight 600 below it, centered. No tagline. Gap 24 below.
2. Button row, two equal-width buttons, gap 8, height 40px:
   - `New project…` primary (rename from "Create project…"). Tooltip `New project (Ctrl+N)`.
   - `Open project…` secondary. Tooltip `Open project (Ctrl+O)`.
3. Gap 24, then a `sectionLabel`-style heading `Recent projects` (15px, 600).
4. Recent list (`QListWidget#recentList`), up to 6 rows, each row 56px tall, radius 10px, background `@surface`, hover `@raised`, gap 4 between rows, no borders, no scrollbar unless more than 6.
   - Row content: project name (13px, `@text`) on top, folder path (11px, `@muted`, elided in the middle) below, and last-opened time right-aligned (11px, `@muted`, short form like `Today`, `Yesterday`, `Nov 12`).
   - Click or Enter opens the project.
   - If the file no longer exists, the row text is `@disabled`, the right label reads `Not found`, clicking does nothing, and right-click offers `Remove from list`.
5. Empty state when there are none: a single 56px dashed-outline row (1px dashed `@line`, radius 10px) with muted text `No recent projects yet`.
6. Last, 24px below, centered, a link-style button `Continue without a project` (`actionRole="link"`). Not a filled button.

### 3.2 Recent projects storage

This is the only new feature in the whole brief besides the collapsible inspector.

- Store in `QSettings` under key `recentProjects`: a list of `{path, openedAt}` maps, newest first, capped at 8, deduplicated by absolute path.
- Update on every successful create and open, from any entry point (welcome page, header menu, shortcut).
- Fill the header menu's `Recent projects` submenu from the same list. Show the project name with the path as the tooltip. Add `Clear recent` at the bottom.

### 3.3 Drag and drop

Dropping a `.openroll` file anywhere on the main window opens it (accept only that extension on `dragEnterEvent`, open on `dropEvent`). Ignore anything else silently.

### Step 3 acceptance

Welcome page shows two buttons, recent list or empty state, and a quiet link. Recent list persists across launches. Naming is "New project…" everywhere (header menu, welcome page, Media panel until Step 6 removes it).

---

## Step 4: Transcode (`src/proxy_widget.cpp`)

Biggest improvement per hour of work. Keep the engine calls, signals, and the test-visible API unchanged.

### 4.1 Layout: two panes plus a queue

```
+--------------------------------------+  +-----------------------------+
| Clips            3 clips · 0 selected|  | Export settings             |
| [ ] name                    codec dur|  | Preset        [ProRes Proxy v]|
| [ ] name                             |  | Max height    [Up to 1080 v] |
| [ ] name                             |  | Output folder [~/Exports  Choose…]|
| Add files…  Remove added file        |  | 3 clips · ProRes Proxy · MOV|
+--------------------------------------+  | [      Export 3 clips      ] |
                                          +-----------------------------+
+---------------------------------------------------------------------+
| Queue                                                                |
| name.braw   Clip 2 of 3 · Encoding · 4:12 left        [====    ]    |
+---------------------------------------------------------------------+
```

- Outer margins 12, gap between cards 8. Left card stretches. Right card is a fixed 300px column. Queue card spans the full width at the bottom.
- All three are `QFrame#card` (`@surface`, radius 10, padding 16).

### 4.2 Clips card (left)

- Header row: `Clips` (`sectionLabel`) on the left; on the right a muted small `3 clips · 0 selected` that updates live, then a `Select all` link-style button.
- List rows 40px tall. Replace the default checkbox with a delegate-painted one, because QSS cannot draw a checkmark without an image resource and we are not adding a `.qrc`. Delegate paints: a 16px rounded square (radius 4), `@line` outline when unchecked, `@accent` fill with a painted white polyline checkmark when checked. Keep the model's check state exactly as it works now so the existing logic and tests are unaffected.
- Row text: filename elided in the middle at body size. If duration and codec already exist in the project clip data, show them right-aligned in muted small. Do not add any new probing to get them.
- Offline clips (not selectable today): text `@disabled`, small `Offline` tag on the right, unchecked and non-interactive.
- Selected-row highlight uses `@accentSoft`. Hover uses `@raised`.
- Footer inside the card, left-aligned: `Add files…` (secondary) and `Remove added file` (ghost, disabled until an added file is selected). Gap 8.

### 4.3 Export settings card (right)

Stacked fields, each a small muted label (11px) above its control, gap 4 between label and control, gap 12 between fields:

1. `Preset`: combo box, full width.
2. `Max height`: combo box, full width.
3. `Output folder`: a read-only elided path line (`@raised` background, radius 6, 32px high) with a `Choose…` button to its right. When unset, the line shows `Not set` in `@warning`.

Below the fields, a muted small summary line: `3 clips · ProRes Proxy · MOV · up to 1080 px` (updates live; shows `No clips selected` when none).

Then, anchored to the bottom of the card, one full-width, 40px primary button:
- Label is `Export N clips` or `Export 1 clip`, live.
- Disabled until at least one clip is checked and an output folder is set. When disabled, the label is `Export` and a one-line reason sits directly above it in muted small: `Select at least one clip` or `Choose an output folder`.
- While exporting, this button is replaced in the same slot by a full-width `Cancel` secondary button.

Delete the loose status labels `Choose an output folder.` and `Ready to export`. Their information now lives in the disabled-reason line and the queue card.

### 4.4 Queue card (bottom)

- Title `Queue` (`sectionLabel`).
- Idle: one muted line `Exports appear here.` and the card is 72px tall.
- Running: line one shows the active filename (elided) on the left and `Clip 2 of 3 · Encoding · 4:12 left` in muted small on the right. Line two is the slim 6px progress bar. Set `setTextVisible(false)`. Never put text inside the bar. Bind to the same progress and phase data the widget already computes.
- Results: the existing results text widget stays, restyled: no border, `@bg` background, radius 10, monospace 12px (`QFontDatabase::FixedFont`), max height 160px, and it is hidden until the first result or error arrives. Completed exports show in `@success`, errors in `@danger`.

### Step 4 acceptance

`tests/transcode_project.cpp` passes. Export works end to end with a BRAW clip and a normal clip. The disabled-reason line is always accurate. No text inside the progress bar. No loose status labels remain.

---

## Step 5: Ingest (`src/ingest_widget.cpp`)

Keep all ingest logic, the drop handling, the SHA-256 verify flow, report generation, and signals.

### 5.1 Layout

A single column, max width 880px, horizontally centered, margins 24 left and right, 16 top. Scroll if the window is short. Three numbered sections, gap 16 between them. Replace the three `QGroupBox#ingestSection` boxes entirely.

Each section is:
- a header row: a 20px circular badge (`@raised` background, `@muted` number, 11px, 600) followed by the section title (`sectionLabel`) with gap 8, then
- a `QFrame#card` below it (gap 8, padding 16).

Titles: `Source`, `Destinations`, `Verify`. Numbers 1, 2, 3.

### 5.2 Source card

- Drop zone: keep `QLabel#sourceDrop`, now 120px tall, radius 14, 1px dashed `@line` border, transparent fill.
- Empty text: `Drop a card, folder or file here` (13px) with a muted small line `or choose one below`.
- With a source: filename on the first line (elided in the middle, 15px, 600) and a muted small line `1 file · 797.4 MiB`. Fix pluralization: `1 file`, `N files`. Format size to one decimal.
- Drag-over state: set a dynamic property `dragActive=true` and style it solid 1px `@accent` border with `@accentSoft` fill. Reset on leave and drop.
- Under the drop zone, right-aligned, gap 8: `Choose file…` and `Choose folder…`, both secondary.

### 5.3 Destinations card

- Remove the fixed or minimum height that makes this box about 350px tall. It sizes to its content.
- Empty: one dashed row (56px, radius 10, 1px dashed `@line`) containing muted `Add a backup destination` and an `Add destination…` secondary button on its right.
- With destinations: one row per destination, 48px, `@raised` background, radius 10. Row shows the destination path (elided) and the **Final copy path** that exists today as a muted small second line. Keep whatever per-row controls exist now; restyle only.
- Below the rows, `Add destination…` as a ghost button.

### 5.4 Verify card

- Left: a status line in muted small that always explains the current state, for example `Add a backup destination to continue`, `Ready: 1 source, 2 destinations`, or the running phase.
- Right: `Start verified copy`, the primary button, 40px tall, disabled until the source and at least one destination exist. This is the only accent-filled button on this screen.
- While running, show one slim 6px progress bar for the overall job with a muted small line `42% · 187 MB/s · 3:10 left` (use existing counters). Under it, one row per destination with its own 6px bar and path, if per-destination progress already exists. If it does not, do not invent it.
- On success, show `All destinations verified` in `@success` and an `Open report folder` secondary button. On failure, show the message in `@danger` and keep the report button.

### Step 5 acceptance

`tests/ingest.cpp` and `tests/ingest_link.cpp` pass. A drag-over visibly highlights the drop zone. Page height is determined by content. Only one accent button visible.

---

## Step 6: Media and Monitor (`src/media.cpp`, `src/playback.cpp` UI parts)

Do not touch scanning, database, or playback logic. Layout and styling only. One `MediaWorkspace` serves both Media and Monitor via `setMonitorMode()`; keep that.

### 6.1 Panels and spacing

- Outer margins 12, gap 8 everywhere (set `QSplitter::setHandleWidth(8)`, handle transparent).
- Left panel, viewer panel, inspector and clip strip are all `QFrame#mediaPanel`: `@surface`, radius 10, no border, padding 12.

### 6.2 Left panel: project media

- Width: default 240, min 200, max 320.
- Header row: `Project media` (`sectionLabel`) left, a 24x24 ghost icon button `+` right with tooltip `Add media folder…`. While a scan runs, swap that button for a link-style `Cancel scan`.
- Delete the three stacked buttons (`Add media folder…`, `New project…`, `Open project…`). Those actions live in the header menu and the `+`.
- Folder tree: row height 28, no branch lines, radius 6 selection in `@accentSoft`, muted folder icons.
- Empty state when no project: centered in the panel, muted `Create or open a project to add media`, no extra buttons.
- The status label stays at the bottom, 11px muted, hidden when empty.

### 6.3 Center: viewer

- The viewer sits inside a `QFrame#viewerFrame` with `@surface` background, radius 10, and 6px inner margin so the square black video never touches the rounded corners.
- Remove the top text bar `Choose a video, audio, or Blackmagic RAW clip.` and the `Open media…` button (now `Open clip…`, Ctrl+Shift+O, in the header menu).
- Replace with a slim 32px toolbar row above the video: current clip name left (13px, elided), and on the right a checkable 28x28 ghost icon button that toggles the inspector (tooltip `Metadata (I)`, shortcut `I`).
- Empty state drawn over the black area: muted `Select a clip`, centered. Only visible when nothing is loaded.
- Scrubber directly under the video, full width, using the slider QSS from Step 1. Make the slider's clickable area 20px tall even though the groove is 4px (give the widget `setFixedHeight(20)`).
- Control bar below the scrubber, one row, height 44, three zones:
  - Left: timecode `HH:MM:SS:FF / HH:MM:SS:FF`, small, `QFontDatabase::FixedFont`, `@muted` for the total and `@text` for the current. Reuse the existing formatter from `playback_time.hpp`.
  - Center: transport. Previous frame and next frame are 32x32 ghost buttons, radius 16. Stop is 32x32 ghost. Play/pause is a 36x36 circular button filled with `@accent` and `@onAccent` icon, the primary action on this screen. Gap 8. Keep `objectName` `transportButton` for the ghosts.
  - Right: a small speaker icon, a 90px volume slider, and a 28x28 ghost fullscreen button. Remove the word `Volume`.
- All existing shortcuts stay: Space, comma, period, F, Escape.

### 6.4 Right: metadata inspector

- Width fixed at 280, hidden by default in Monitor mode, toggle-able in Media mode. Remember the last visibility.
- Heading `Metadata` (`sectionLabel`).
- Rows are two-column key/value: key 11px `@muted`, value 12px `@text`, selectable text, 8px between rows. Group into small muted section headers if the data already has natural groups (file, video, audio, timecode). Do not invent groups.
- Empty state: muted `Select a clip to see its metadata`, centered.

### 6.5 Bottom: clip strip

- Panel with a 40px toolbar row, then the cards.
- Toolbar: a pill view toggle on the left (two buttons, grid and list, replacing the unicode glyphs with painted 16px icons; container `@raised`, radius 14, 3px padding; checked segment `@accentSoft`), then the search field (`Search clips`, `setClearButtonEnabled(true)`, 28px high), stretch, then `3 clips` in muted small.
- Clip cards via a `QStyledItemDelegate`, 164px wide:
  - Thumbnail with 16:9 ratio, painted through a `QPainterPath` with 8px rounded clip.
  - Below it, one row: filename elided in the middle (12px) left, duration muted small right.
  - No border. Hover: `@raised` card background (radius 10). Selected: 2px `@accent` outline at radius 10, thumbnail unchanged, no fill.
  - Card total height about 150. Set the strip's fixed height to fit the toolbar plus one row of cards (about 208). Replace the existing hardcoded 139.
- List mode keeps current columns, restyled with the tree/list QSS from Step 1.

### 6.6 Monitor mode

`setMonitorMode(true)` hides the left panel, inspector and clip strip. What remains: the viewer frame with the same toolbar row, scrubber and control bar. The timecode is 14px in this mode. The viewer fills the window with 12px outer margin. Fullscreen (F) still shows the video alone.

### Step 6 acceptance

`tests/playback_time.cpp`, `tests/project.cpp`, and the smoke test pass. Media and Monitor share one widget and switching is instant. Play is the only accent-filled control while a clip is loaded. No panel has a border.

---

## Step 7: Polish pass

1. Terminology: `New project…` and `Open project…` everywhere, sentence case for all labels, trailing `…` only where a dialog opens.
2. Every icon-only button has a tooltip with its shortcut where one exists.
3. Tab order is logical in each workspace and focus is visible (1px `@accent` border) on every focusable control except the workspace switcher.
4. Audit: grep the codebase for any hex color or `px` value outside `theme.hpp` and the allowed spacing scale (4, 8, 12, 16, 24 plus the fixed sizes listed above). Move strays to tokens.
5. Check every screen at 1280x790 and at a narrow 960 wide. Nothing overlaps, nothing clips, long filenames elide.

## Global acceptance checklist

- [ ] No `QGroupBox` with a visible border remains.
- [ ] No bottom navigation bar.
- [ ] Exactly one accent-filled button per screen: New project (welcome), Start verified copy (Ingest), Play (Media/Monitor), Export (Transcode).
- [ ] Every radius is one of 6, 10, 14 or pill.
- [ ] Every gap and padding is 4, 8, 12, 16 or 24 (plus the listed fixed control sizes).
- [ ] Empty states exist and tell the user what to do next.
- [ ] No duplicated controls: project actions only in the header menu and `+`, project name only in the header button.
- [ ] No text inside progress bars.
- [ ] `meson test -C build --print-errorlogs` passes at every step.

## Out of scope, ask me first

Rounding or replacing the native window frame, any QML, light theme, new features beyond recent projects and the collapsible inspector, any change to ingest, transcode, scanning or playback behavior.
