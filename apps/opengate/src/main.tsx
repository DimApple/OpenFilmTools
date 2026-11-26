import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { App } from './App';
import { startAutosave, startMoneyFormat } from './store/project';
import { startTheme } from './theme';
import { installWriteGuard } from './sync/guard';
import { startFileAutosave } from './io/fileSave';
import { startOffline } from './offline';
import { startCardTables } from './ui/cardTables';
import { startKeyboardInset } from './ui/responsive';
import { applyPrefs } from './prefs';
import '@fontsource/inter/400.css';
import '@fontsource/inter/500.css';
import '@fontsource/inter/600.css';
import '@fontsource/inter/700.css';
import '@fontsource/courier-prime/400.css';
import '@fontsource/courier-prime/400-italic.css';
import '@fontsource/courier-prime/700.css';
import '@fontsource/courier-prime/700-italic.css';
import './styles.css';

if (location.protocol === 'opengate:') document.title = 'OpenGate Levi Edition';

startAutosave();
startMoneyFormat();
startTheme();
installWriteGuard();
startFileAutosave();
startOffline();
startCardTables();
startKeyboardInset();
applyPrefs();
// Dev only: opengateAudit() in the console lists text running off its button, menu or the screen.
if (import.meta.env.DEV) void import('./ui/overflowAudit').then((m) => m.installOverflowAudit());

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
