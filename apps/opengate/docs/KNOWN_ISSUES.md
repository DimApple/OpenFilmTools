# Known issues and verification gaps

- Windows installer launch was reported to fail. This baseline does not include the installer recipe or a Windows test environment. Root cause is undetermined.
- Flatpak manifest and published Flatpak were unavailable in the baseline source. Flatpak installation and launch remain unverified.
- The debounced IndexedDB autosave and manual flush both call the database save path without explicit write serialization. Manual flush can mark the store saved after a newer edit. The error path returns to dirty state and the UI has no separate Save Failed state. Asynchronous before-unload flushing needs exit/recovery testing.
- The current shell exposes Write and Plan on the bottom bar. The stage selector, independent Storyboard and Deliver workspaces, Focus Mode and hybrid layout persistence remain planned.
- Existing automated tests leave interactive drawing, desktop print/PDF, older production project migration, installed-app launch and workspace timings unverified.
