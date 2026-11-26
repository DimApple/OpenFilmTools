# Phase 0 recovery audit

The Streamlined source archive matches the tracked source tree at `be3a44747b9ebe7c964b449f77227f9fa1078257`, recovered from the workspace's Git history bundle. The history begins at `0e62b4a`, labeled upstream Open Gate 0.9.0. The comparison in this audit uses that snapshot. The separate original ZIP and installed Windows/Flatpak release files were unavailable for direct comparison. The connected GitHub repository search returned no OpenGate match.

The baseline includes five Levi Edition commits covering Electron packaging, storyboard references and coverage, Write and Plan, the split planning workspace, and screenplay saving and storyboard export. Use `git diff 0e62b4a be3a447` to review the exact changes.

## Milestone 1 implementation order

1. Establish a regression baseline in the Electron shell. Record cold and warm navigation timings on a synthetic project with multiple scenes, shots, and boards. Include reopen, rapid saves, and switching with pending edits.
2. Build the stage/workspace registry over the existing tab routing and access checks in `src/App.tsx`. Route Write to screenplay, Plan to shot planning, Storyboard to the existing native editor, and Deliver to existing reports and export actions. Keep existing tabs reachable through Tools and preserve contextual links, phone navigation, and collaboration access checks. Store validated stage and workspace preferences separately from project data.
3. Reuse the existing workspace panels and preferences. Define workspace widths and collapse state independently of a temporary Focus Mode overlay. Preserve keyboard-accessible resize, collapse, reset, and previous-width restoration. Keep screenplay page dimensions and pagination stable.
4. Put persistence coordination in the existing project store and file save paths. Serialize writes by project identity, mark only the written snapshot saved, preserve newer edits, and surface errors. Test overlap, failures, project switches, and migration-free reopening. Keep script session flushing ahead of transitions.
5. Move expensive mounts behind explicit workspace activation. Preserve write selection, scroll, and script binding deliberately and keep shared-project behavior. Formalize typed interfaces over the existing project, navigation, preferences and job/error responsibilities. Leave external adapters and media engines for their respective milestones.
6. Run the full regression suite and supported-platform packaging/launch checks. Compare timing samples with the baseline before evaluating Milestone 1 acceptance.
