# Milestone 1 desktop preview

The desktop top bar selects Pre-Production, Production, Post-Production, or All Tools. The bottom bar lists Write, Plan, Storyboard, and Deliver according to the selected stage and project access. Tools opens the existing project sidebar. Plan retains Shot List, Storyboard, and Split views, and Deliver opens the existing reports and export hub.

Write's scene navigator and Plan's inspector each remember their width and visibility across project opens. Use Layout for the keyboard-adjustable width and Reset Layout. Focus Mode temporarily hides the Tools sidebar, Write navigator, and Plan inspector. Toggle it with the toolbar button or Ctrl+Shift+F.

The shared Zustand project, ProseMirror session, existing IndexedDB schema and portable bundle format are retained. The local store serializes project writes, tracks save status against the exact snapshot written, and surfaces failures for retry. Returning Home flushes pending edits and checks for edits made while a save was underway. Workspace transitions flush the screenplay session.

## Verification

Headless Linux Electron opened the built-in sample production, switched between Plan and Write, confirmed stage routing for Production, Post-Production and Pre-Production, and toggled Focus Mode with its button and shortcut.

| Measurement | Phase 0 | Milestone 1 preview |
| --- | ---: | ---: |
| First Write-to-Plan switch | 173 ms | 74 ms |
| Subsequent alternating switches | 39–96 ms | 51–99 ms |

One headless run per build on the same machine. Each sample spans button activation through two animation frames. This covers Write and Plan on the built-in sample. The measurements do not establish general typing latency, autosave latency or performance across all workspaces.

## Open acceptance checks

- The user confirmed that the purple Milestone 1 portable package launched on Windows. Windows functional regression checks remain open. Windows Setup and Flatpak recipes are absent from the recovered source.
- Graphical checks remain for panel sizing at different window widths, layout persistence through restart, PDF/print output, storyboard drawing and reference handling, and reopening representative pre-existing projects.
- The local Write editor remains mounted while using Plan or Storyboard to preserve position and binding. Full independent workspace loading and inactive resource suspension need separate verification and implementation.
- The shared background job lifecycle, external adapter contracts, and interrupted-save recovery beyond IndexedDB transactions remain open. No future media or drawing tools were introduced.

The production build passed, and Vitest passed all 284 tests across 40 files, including delayed-write ordering and retry-after-failure regressions.

## Workspace loading follow-up

Plan, Storyboard, and Deliver now load on first use. Plan's Shot List view does not load the storyboard editor until Storyboard or Split is selected. The production build emits separate modules and the main JavaScript chunk decreased from about 2,321 KB to 2,217 KB before compression.

A packaged headless Linux check confirmed the first-use module requests and rendered the Plan shot list, Split, standalone Storyboard, Deliver, and Write. The full suite passed all 284 tests. The local Write editor still stays mounted across Plan and Storyboard to preserve position and binding.
