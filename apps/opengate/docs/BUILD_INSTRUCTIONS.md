# Build instructions

From the source root on Linux or Windows, use Node.js 22.12+ (baseline checked with Node 24.22.3) and npm. Install dependencies from the lockfile and build the renderer.

```sh
npm ci
npm run build
npm test -- --maxWorkers=2 --minWorkers=1
npm run desktop
```

The first Electron launch or packaging run can download the version pinned in `package.json`. Linux needs a graphical session and Electron's shared-library dependencies. To develop the web renderer with hot reload, use `npm run dev`.

```sh
npm run desktop:package -- linux
npm run desktop:package -- win32
```

These commands produce portable x64 application directories in `desktop-build/`. Run the Linux executable or the Windows `.exe` on its target OS. `OPENGATE_ELECTRON_ZIPS` can point to a directory containing the required predownloaded Electron ZIPs. The source package has no Flatpak or Windows Setup recipe.

The Vite bundle includes a wall-clock-generated service worker cache version, so repeat builds are not byte-identical. The renderer uses relative asset paths for desktop packaging.

## Phase 0 results

- Linux x64, Node 24.22.3. `npm ci` passed with dependency deprecation and lifecycle warnings.
- `npm run build` passed. Vite reported a PDF.js eval warning, mixed static/dynamic imports, and a main chunk of about 2.32 MB before gzip.
- Vitest passed 39 test files and 282 tests.
- `node desktop/package.mjs linux` produced the portable Linux x64 directory. The packaged executable loaded the OpenGate home screen in Electron with `--ozone-platform=headless --disable-gpu` and a fresh profile. Normal graphical launch was unavailable in the shell due to no X display.
- Windows Setup and Flatpak launch tests remain pending. The headless Linux check verified startup and home-screen rendering.
