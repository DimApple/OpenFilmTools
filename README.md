# Open filmmaking tools

A collection of OpenGate Levi Edition, OpenTake and OpenRoll. Each app remains independent.

## Publish
1. Review the source and third-party licenses before making the repository public. Check for private data and credentials; this is not a full security or license audit.
2. Create a GitHub repository and push the contents of this folder to its main branch.
3. In Settings > Pages, choose Deploy from a branch, main, /docs, and save.
4. Open the Pages URL and test the OpenTake web app and source downloads.

For a local preview: `python -m http.server 8000 --directory docs`, then open http://localhost:8000/.

## Inventory
- apps/opengate: Milestone 2 Storyboard source, not the older Milestone 1 installer source.
- apps/opentake: GitHub Pages source and existing compiled web build.
- apps/opentake-android: Android Preview 2 source.
- apps/openroll: 0.17.2 Windows Beta source checkpoint; later installer-specific changes may not be represented.
- docs: static website, OpenTake deployment, source downloads and Android preview APK.

Desktop installers are not included. Use GitHub Releases for large binaries after verifying licensing, matching source and clean-system installation. In particular, obtain Blackmagic RAW SDK separately and review redistribution terms. No application was rebuilt or runtime-tested during this assembly. Original app documentation and license files remain in their respective directories.
