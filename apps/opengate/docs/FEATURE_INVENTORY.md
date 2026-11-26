# Feature inventory

| Flow | Upstream snapshot | Levi Edition baseline | Verification scope |
| --- | --- | --- | --- |
| Screenplay | ProseMirror editor, formatting, pagination and PDF | Existing editor plus coverage decorations, split planning integration, and pagination changes | Model and export tests. Desktop editing, pagination and print still need end-to-end checks. |
| Create, open, save | IndexedDB, project imports, portable bundles, file saves | Electron-local database, revised Save and Save As behavior | Format and storage-related tests. Installed-app reopen and legacy projects need manual checks. |
| Shot list | Scene-linked shots and planning | Adds imports and coverage editing with shot selection | Unit coverage. Interactive import and link edits remain unverified. |
| Lined script | No Levi coverage workflow | Adds shot marks in ProseMirror, editing controls and lined screenplay PDF | Coverage suite. Desktop visual PDF and cross-page coverage need review. |
| Storyboard | Native frames and shot associations | Adds reference-import changes, split Plan layout and revised exports | Storyboard tests. Drawing, reference insertion and visual PDF need runtime verification. |
| Autosave | Debounced database save and optional portable file autosave | Retains autosave with desktop Save fallback | Save ordering and failure cases remain open. |
| Desktop shell | Browser/PWA shell | Electron dark shell, Write/Plan navigation, Tools drawer | Stage selector, independent Storyboard and Deliver navigation, hybrid panels and Focus Mode are planned. |
| Installers | Browser install | Electron packager for portable x64 directories | No installer recipes in this source tree. Installed Windows and Flatpak releases are unverified. |
