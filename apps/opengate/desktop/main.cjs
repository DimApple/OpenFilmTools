const { app, BrowserWindow, net, protocol, shell } = require('electron');
const path = require('node:path');
const { pathToFileURL } = require('node:url');

app.setName('opengate-levi-edition');
app.setAppUserModelId('opengate-levi-edition');

const scheme = 'opengate';
const startUrl = `${scheme}://desktop/`;
const renderer = path.resolve(__dirname, '..', 'dist');

protocol.registerSchemesAsPrivileged([{
  scheme,
  privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true, stream: true },
}]);

function serveApp(request) {
  try {
    const url = new URL(request.url);
    if (url.hostname !== 'desktop') return new Response(null, { status: 404 });
    const pathname = decodeURIComponent(url.pathname);
    const file = path.resolve(renderer, '.' + (pathname === '/' ? '/index.html' : pathname));
    const relative = path.relative(renderer, file);
    if (!relative || relative === '..' || relative.startsWith('..' + path.sep) || path.isAbsolute(relative)) {
      return new Response(null, { status: 404 });
    }
    return net.fetch(pathToFileURL(file).href);
  } catch {
    return new Response(null, { status: 400 });
  }
}

function openWindow() {
  const window = new BrowserWindow({
    title: 'OpenGate Levi Edition',
    width: 1440,
    height: 950,
    icon: path.join(renderer, 'icons', 'icon-192.png'),
    autoHideMenuBar: true,
    webPreferences: { sandbox: true, contextIsolation: true, nodeIntegration: false },
  });
  window.webContents.on('will-navigate', (event) => event.preventDefault());
  window.webContents.on('will-redirect', (event) => event.preventDefault());
  window.webContents.setWindowOpenHandler(({ url }) => {
    if (/^https?:\/\//i.test(url)) void shell.openExternal(url).catch(console.error);
    return { action: 'deny' };
  });
  window.webContents.session.setPermissionRequestHandler((_contents, _permission, callback) => callback(false));
  window.webContents.session.setPermissionCheckHandler(() => false);
  void window.loadURL(startUrl).catch(console.error);
}

app.whenReady().then(() => {
  protocol.handle(scheme, serveApp);
  openWindow();
  app.on('activate', () => { if (!BrowserWindow.getAllWindows().length) openWindow(); });
});
app.on('window-all-closed', () => { if (process.platform !== 'darwin') app.quit(); });
