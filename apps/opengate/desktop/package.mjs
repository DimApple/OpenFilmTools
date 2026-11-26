import { packager } from '@electron/packager';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const platforms = process.argv.slice(2);
if (platforms.some((platform) => !['linux', 'win32'].includes(platform))) {
  throw new Error('Choose linux or win32.');
}
const stage = await mkdtemp(join(tmpdir(), 'opengate-package-'));
try {
  const { version, devDependencies } = JSON.parse(await readFile('package.json', 'utf8'));
  for (const entry of ['dist', 'desktop/main.cjs', 'LICENSE', 'NOTICE']) {
    await cp(entry, join(stage, entry), { recursive: true });
  }
  for (const font of ['inter', 'courier-prime']) {
    await cp(join('node_modules', '@fontsource', font, 'LICENSE'), join(stage, `${font}-LICENSE.txt`));
  }
  await writeFile(join(stage, 'package.json'), JSON.stringify({
    name: 'opengate-levi-edition', productName: 'OpenGate Levi Edition', version, main: 'desktop/main.cjs',
  }));
  const paths = await packager({
    dir: stage,
    out: 'desktop-build',
    name: 'OpenGate-Levi-Edition',
    platform: platforms.length ? platforms : ['linux', 'win32'],
    arch: 'x64',
    electronVersion: devDependencies.electron,
    electronZipDir: process.env.OPENGATE_ELECTRON_ZIPS,
    asar: true,
    overwrite: true,
    prune: false,
  });
  console.log(paths.join('\n'));
} finally {
  await rm(stage, { recursive: true, force: true });
}
