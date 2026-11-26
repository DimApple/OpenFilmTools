import type { PlanLayout } from './tabs/PlanWorkspace';
import { usePrefs } from './prefs';
import { Tour, TourOffer } from './tour/Tour';
import { PlanBudgetSummary } from './components/PlanBudget';
import { HelpDialogs } from './help/HelpMenu';
import { useTabLock } from './store/tabLock';
import { BudgetCapBar } from './components/BudgetHelp';
import { tabHidden, useSync } from './sync/state';
import { lazy, Suspense, useCallback, useEffect, useState } from 'react';
import { useStore, flushSave } from './store/project';
import { loadProject } from './store/db';
import { session } from './script/session';
import { HomeScreen } from './tabs/HomeScreen';
import { WriteTab } from './tabs/WriteTab';
import { BreakdownTab } from './tabs/BreakdownTab';
import { ShotsTab } from './tabs/ShotsTab';
import { ScheduleTab } from './tabs/ScheduleTab';
import { CalendarTab } from './tabs/CalendarTab';
import { SetupTab } from './tabs/SetupTab';
import { DoodView } from './tabs/reports/DoodView';
import { BudgetTab } from './tabs/budget/BudgetTab';
import { ExpensesTab } from './tabs/budget/ExpensesTab';
import { CallSheetsView } from './tabs/reports/CallSheetsView';
import { SidesView } from './tabs/reports/SidesView';
import { DocsTab } from './tabs/DocsTab';
import { AVScriptTab } from './tabs/AVScriptTab';
import { ContactsTab } from './tabs/ContactsTab';
import { TasksTab } from './tabs/TasksTab';
import { BoardsTab } from './tabs/boards/BoardsTab';
import { LocationsTab } from './tabs/LocationsTab';
import { CharactersTab } from './tabs/CharactersTab';
import { StoryBibleTab } from './tabs/StoryBibleTab';
import { ShotDesignerTab } from './tabs/designer/ShotDesignerTab';
import { GearTab } from './tabs/budget/GearTab';
import { LicensesTab } from './tabs/budget/LicensesTab';
import { FundingTab } from './tabs/budget/FundingTab';
import { FestivalsTab } from './tabs/release/FestivalsTab';
import { PressKitTab } from './tabs/release/PressKitTab';
import { DistributionTab } from './tabs/release/DistributionTab';
import { SocialTab } from './tabs/release/SocialTab';
import { PaperworkTab } from './tabs/budget/PaperworkTab';
import { ReferenceTab } from './tabs/ReferenceTab';
import { CategoryStyles } from './components/CategoryStyles';
import { ProjectMenu } from './components/ProjectMenu';
import { ProjectSidebar } from './components/ProjectSidebar';
import { SyncIssues } from './components/SyncIssues';
import { PinterestReceiver } from './tabs/boards/PinterestReceiver';
import { DialogHost } from './components/dialogs';
import { UndoRedo } from './components/EditMenu';
import { usePhone, useNarrow, useCompactDesktop } from './ui/responsive';
import { usePhoneTabs, visibleSections, registerSections } from './ui/phoneTabs';
import { PhoneTopbar } from './ui/PhoneChrome';
import { parseStage, stages, workspaces, workspacesFor, type Stage } from './ui/workspaces';
import { PhonePanels } from './ui/PhonePanels';
import { ActionSheetHost } from './components/ActionSheet';
import { TabBoundary } from './components/TabBoundary';
import { JoinHandler } from './sync/JoinHandler';
import { ShareBar } from './components/share/ShareBar';
import { AccessBanner } from './components/share/AccessBanner';
import { startSync, stopSync, currentSync } from './sync/engine';
import { saveToFile, loadFileState, useFile } from './io/fileSave';

const PlanWorkspace = lazy(() => import('./tabs/PlanWorkspace').then(({ PlanWorkspace }) => ({ default: PlanWorkspace })));
const StoryboardTab = lazy(() => import('./tabs/StoryboardTab').then(({ StoryboardTab }) => ({ default: StoryboardTab })));
const ShootTab = lazy(() => import('./tabs/ShootTab').then(({ ShootTab }) => ({ default: ShootTab })));

export type SectionId = 'write' | 'breakdown' | 'budget' | 'plan' | 'visualize' | 'shoot' | 'release' | 'reference' | 'exports' | 'settings';
export type TabId =
  | 'screenplay' | 'av' | 'docs' | 'characters' | 'bible'
  | 'breakdown' | 'stripboard' | 'dood' | 'sides'
  | 'budget' | 'funding' | 'festivals' | 'expenses' | 'gear' | 'licenses' | 'paperwork'
  | 'contacts' | 'calendar' | 'tasks'
  | 'storyboard' | 'boards' | 'shots' | 'designer'
  | 'callsheets' | 'locations' | 'reports'
  | 'presskit' | 'social' | 'distribution'
  | 'reference'
  | 'setup';

export const SECTIONS: { id: SectionId; label: string; tabs: { id: TabId; label: string; hint: string }[] }[] = [
  // In production order: write it, see it, break it down, plan the people and places, cost it, shoot it, release it.
  { id: 'write', label: 'Write', tabs: [
    { id: 'screenplay', label: 'Screenplay', hint: 'Write or import a screenplay' },
    { id: 'characters', label: 'Characters', hint: 'Character builder: traits, alignment, profile and continuity photos' },
    { id: 'bible', label: 'Story Bible', hint: 'The world of the story: overview, story days, places, groups, glossary, rules' },
    { id: 'docs', label: 'Docs', hint: 'Treatments, notes and other documents' },
    { id: 'av', label: 'A/V Scripts', hint: 'Two-column video / audio scripts' },
  ] },
  { id: 'visualize', label: 'Visualize', tabs: [
    { id: 'boards', label: 'Mood Boards', hint: 'Mood boards and whiteboards' },
    { id: 'designer', label: 'Shot Designer', hint: 'Top-down plan per scene: actors, cameras with lens-accurate angles, lights, moves' },
    { id: 'storyboard', label: 'Storyboards', hint: 'Frames for each shot' },
  ] },
  { id: 'breakdown', label: 'Breakdown', tabs: [
    { id: 'breakdown', label: 'Script Breakdown', hint: 'Tag elements in the script' },
    { id: 'shots', label: 'Shot Lists', hint: 'Plan coverage per scene: the setups that drive how long each day takes' },
    { id: 'stripboard', label: 'Stripboard', hint: 'Order scenes into shoot days' },
    { id: 'dood', label: 'Day Out of Days', hint: 'Cast work days across the shoot' },
  ] },
  { id: 'plan', label: 'Plan', tabs: [
    { id: 'contacts', label: 'Contacts', hint: 'Cast, crew, vendors and other contacts' },
    { id: 'locations', label: 'Locations', hint: 'Locations, contacts, permits and photos' },
    { id: 'paperwork', label: 'Paperwork', hint: 'Deal memos, releases signed on a phone or iPad, start forms, unions, clearances and time cards' },
    { id: 'calendar', label: 'Calendar', hint: 'Shoot days, prep, events and tasks' },
    { id: 'tasks', label: 'Tasks', hint: 'Prep to-dos with due dates' },
  ] },
  { id: 'budget', label: 'Budget', tabs: [
    { id: 'budget', label: 'Budget', hint: 'Top sheet, accounts, targets, actuals and versions' },
    { id: 'funding', label: 'Funding', hint: 'How much to raise: from your breakdown to a funding goal, crowdfunding with rewards' },
    { id: 'gear', label: 'Gear', hint: 'Gear manifest: own, rent, borrow or buy — rentals become budget lines' },
    { id: 'expenses', label: 'Expenses', hint: 'Receipts and spending, imported from your bank or card' },
    { id: 'licenses', label: 'Licenses', hint: 'Music, stock footage and other clearances — fees, terms, credit lines, cue sheet' },
  ] },
  { id: 'shoot', label: 'Shoot', tabs: [
    { id: 'callsheets', label: 'Call Sheets', hint: 'Daily call sheets' },
    { id: 'sides', label: 'Script Sides', hint: 'Each day’s pages for cast and crew' },
  ] },
  // Marketing and distribution: it starts in prep, but it's where the film goes last.
  { id: 'release', label: 'Release', tabs: [
    { id: 'festivals', label: 'Festivals', hint: 'Festivals you’re aiming at: deadlines, dates and status, laurels; fees for people with the budget' },
    { id: 'presskit', label: 'Press Kit', hint: 'EPK: logline, synopsis, stills, cast & crew, laurels, tech specs — as a PDF and a one-page website' },
    { id: 'social', label: 'Social & BTS', hint: 'Content calendar tied to milestones, photos and clips, and a releases check for everyone shown' },
    { id: 'distribution', label: 'Distribution', hint: 'Deliverables checklist, sales agents / distributors / platforms, and whether your licences cover them' },
  ] },
  { id: 'reference', label: 'Reference', tabs: [{ id: 'reference', label: 'Reference', hint: 'Story, camera, lenses, lighting, sound, data, production, guilds and unions — explainers, calculators and a library' }] },
  // Shown as buttons next to the sections (⤓ and ⚙), not as sections.
  { id: 'exports', label: 'Exports', tabs: [{ id: 'reports', label: 'Exports', hint: 'Every export in one place: PDFs, spreadsheets, calendars, end credits' }] },
  { id: 'settings', label: 'Settings', tabs: [{ id: 'setup', label: 'Settings', hint: 'This project (shared) and this device (yours)' }] },
];

/** Tabs with a side panel that can be hidden. */
const SIDE_TABS = new Set<string>(['screenplay', 'av', 'docs', 'characters', 'bible', 'breakdown', 'stripboard', 'calendar', 'boards', 'designer', 'callsheets', 'locations', 'reference', 'setup']);

/** Section names when the top bar is tight. */

const ALL_TABS = SECTIONS.flatMap((s) => s.tabs.map((t) => t.id));
registerSections(SECTIONS);
const sectionOf = (t: TabId) => SECTIONS.find((s) => s.tabs.some((x) => x.id === t))!.id;
// Tab ids from before the sections layout.
const LEGACY: Record<string, TabId> = { write: 'screenplay', schedule: 'stripboard', shoot: 'callsheets', setup: 'setup' };

export function App() {
  const leviShell = location.protocol === 'opengate:';
  const project = useStore((s) => s.project);
  usePrefs(); // date / clock / week-start preferences re-render everything
  const hiddenSync = useSync((s) => s.hidden);
  const saveState = useStore((s) => s.saveState);
  // Read-only while another tab edits this project: re-render so every screen picks it up.
  useTabLock((s) => s.readOnly);
  const phone = usePhone();
  const narrow = useNarrow();
  const phoneTabs = usePhoneTabs();
  const [tab, setTabState] = useState<TabId>(() => {
    const saved = localStorageGet('tab') ?? '';
    const t = (LEGACY[saved] ?? saved) as TabId;
    return ALL_TABS.includes(t) ? t : 'screenplay';
  });
  const [stage, setStageState] = useState(() => {
    const stored = parseStage(localStorageGet('stage'));
    return workspaces.some((entry) => entry.tab === tab) &&
      !workspacesFor(stored).some((entry) => entry.tab === tab) ? 'all' : stored;
  });
  const [focus, setFocus] = useState(false);
  const layoutKey = (current: TabId) => 'workspace.width.' + current;
  const defaultWidth = tab === 'screenplay' ? 240 : 300;
  const [panelWidths, setPanelWidths] = useState<Partial<Record<TabId, number>>>({});
  const savedWidth = panelWidths[tab] ?? Number(localStorageGet(layoutKey(tab)));
  const panelWidth = savedWidth >= 180 && savedWidth <= 420 ? savedWidth : defaultWidth;
  function resizePanel(width: number) {
    setPanelWidths((current) => ({ ...current, [tab]: width }));
    localStorageSet(layoutKey(tab), String(width));
  }
  function resetLayout() {
    resizePanel(defaultWidth);
    setHiddenSides((current) => {
      const next = current.filter((entry) => entry !== tab);
      localStorageSet('sideHidden', JSON.stringify(next));
      return next;
    });
    if (tab === 'shots') { rememberPlan('shots'); localStorageSet('planInspector', 'open'); }
    setLayoutReset((current) => current + 1);
  }
  const [layoutReset, setLayoutReset] = useState(0);

  const stageWorkspaces = (selected: Stage) => workspacesFor(selected).filter(({ tab }) => !tabHidden(tab));
  const availableWorkspaces = stageWorkspaces(stage);
  function rememberStage(next: Stage) {
    setStageState(next);
    localStorageSet('stage', next);
  }
  function selectStage(next: Stage) {
    rememberStage(next);
    const available = stageWorkspaces(next);
    if (available.some((workspace) => workspace.tab === tab)) {
      localStorageSet('workspace.' + next, tab);
      return;
    }
    const previous = localStorageGet('workspace.' + next);
    const entry = available.find((workspace) => workspace.tab === previous) ?? available[0];
    if (entry) selectWorkspace(entry.tab, next);
  }
  function selectWorkspace(next: typeof workspaces[number]['tab'], selected = stage) {
    localStorageSet('workspace.' + selected, next);
    setTab(next, true, selected);
  }
  // Each section remembers its last sub-tab.
  const [lastInSection, setLastInSection] = useState<Partial<Record<SectionId, TabId>>>(() => {
    try { return JSON.parse(localStorageGet('sections') ?? '{}') ?? {}; } catch { return {}; }
  });
  const [planLayout, setPlanLayout] = useState<PlanLayout>(() => {
    const saved = localStorageGet('planLayout');
    return saved === 'split' || saved === 'storyboard' || saved === 'shots' ? saved : tab === 'storyboard' ? 'storyboard' : 'shots';
  });
  function rememberPlan(layout: PlanLayout) {
    setPlanLayout(layout);
    localStorageSet('planLayout', layout);
  }
  const setTab = (t: TabId, keepPlanLayout = false, selected = stage) => {
    if (!keepPlanLayout && t === 'shots') rememberPlan('shots');
    session.flush();
    if (leviShell && workspaces.some((entry) => entry.tab === t)) {
      const destination = workspacesFor(selected).some((entry) => entry.tab === t) ? selected : 'all';
      if (destination !== selected) rememberStage(destination);
      localStorageSet('workspace.' + destination, t);
    }
    setTabState(t);
    setLastInSection((m) => ({ ...m, [sectionOf(t)]: t }));
  };
  const [planScene, setPlanScene] = useState('');
  const [toolsOpen, setToolsOpen] = useState(() => localStorageGet('leviTools') === 'open');
  const isPlan = tab === 'shots';
  const isWrite = tab === 'screenplay';
  const canPlan = !tabHidden('shots') || !tabHidden('storyboard');
  useEffect(() => { setPlanScene(''); }, [project?.id]);
  function openPlan(sceneId?: string) {
    if (!canPlan) return;
    if (sceneId) setPlanScene(sceneId);
    if (!tabHidden('shots')) setTab('shots', true);
    else setTab('storyboard', true);
  }
  function toggleTools() {
    const next = !toolsOpen;
    setToolsOpen(next);
    localStorageSet('leviTools', next ? 'open' : 'closed');
  }
  // Shared scripts use the existing single-view binding. Local scripts keep Write mounted
  // across both workspaces so the page and its scroll position survive a switch.
  const persistentWrite = leviShell && !project?.share && !tabHidden('screenplay') && (isWrite || isPlan || tab === 'storyboard');
  const section = sectionOf(tab);
  // A section's last tab, unless that tab has since moved to another section (then its first tab).
  const lastTabIn = (x: { id: SectionId; tabs: { id: TabId }[] }) => { const t = lastInSection[x.id]; return t && x.tabs.some((y) => y.id === t) ? t : x.tabs[0].id; };
  const openBoards = useCallback(() => setTab('boards'), [stage]); // eslint-disable-line react-hooks/exhaustive-deps
  // Hide the side panel to give the work more room (remembered per tab on this device).
  const [hiddenSides, setHiddenSides] = useState<string[]>(() => { try { const value: unknown = JSON.parse(localStorageGet('sideHidden') ?? '[]'); return Array.isArray(value) ? value.filter((entry): entry is string => typeof entry === 'string') : []; } catch { return []; } });
  const sideHidden = focus || hiddenSides.includes(tab);
  // The project sidebar: open or icons, remembered on this device; until you choose, smaller windows start with icons.
  const compact = useCompactDesktop();
  const [sidebarChoice, setSidebarChoice] = useState<string | null>(() => localStorageGet('sidebar'));
  const sidebarCollapsed = sidebarChoice ? sidebarChoice === 'icons' : compact;
  const toggleSidebar = () => { const next = sidebarCollapsed ? 'open' : 'icons'; localStorageSet('sidebar', next); setSidebarChoice(next); };
  const toggleSide = () => setHiddenSides((h) => { const next = h.includes(tab) ? h.filter((x) => x !== tab) : [...h, tab]; localStorageSet('sideHidden', JSON.stringify(next)); return next; });
  useEffect(() => {
    const key = (e: KeyboardEvent) => { if ((e.metaKey || e.ctrlKey) && e.key === '\\' && SIDE_TABS.has(tab)) { e.preventDefault(); toggleSide(); } };
    window.addEventListener('keydown', key);
    return () => window.removeEventListener('keydown', key);
  }); // eslint-disable-line react-hooks/exhaustive-deps

  // A tab hidden by this person's link: go to the first one they can see.
  useEffect(() => {
    if (!tabHidden(tab)) return;
    const first = SECTIONS.flatMap((x) => x.tabs).find((t) => !tabHidden(t.id));
    if (first) setTab(first.id);
  }, [hiddenSync, tab, stage]); // eslint-disable-line react-hooks/exhaustive-deps
  // Other tabs can jump somewhere (e.g. a cast row's "Profile →" opens Characters).
  useEffect(() => {
    const go = (e: Event) => { const t = (e as CustomEvent).detail as TabId; if (ALL_TABS.includes(t)) setTab(t); };
    window.addEventListener('opengate:goto', go);
    return () => window.removeEventListener('opengate:goto', go);
  }, [stage]); // eslint-disable-line react-hooks/exhaustive-deps

  // Shared projects: keep the open project connected to its room (works offline too).
  const shareKey = project?.share ? `${project.id}|${project.share.room}|${project.share.token}|${project.share.format ?? 1}` : '';
  useEffect(() => {
    const p = useStore.getState().project;
    if (shareKey && p?.share) void startSync(p);
    else stopSync();
  }, [shareKey]);
  useEffect(() => () => stopSync(), []);
  useEffect(() => { if (project?.id) void loadFileState(project.id); }, [project?.id]); // eslint-disable-line react-hooks/exhaustive-deps
  const file = useFile();
  useEffect(() => {
    const t = SECTIONS.flatMap((s) => s.tabs).find((x) => x.id === tab);
    currentSync()?.setTab(t?.label ?? tab);
  }, [tab, shareKey]);

  useEffect(() => { localStorageSet('tab', tab); }, [tab]);
  useEffect(() => { localStorageSet('sections', JSON.stringify(lastInSection)); }, [lastInSection]);

  // Reopen the last project on launch; "All projects" clears this.
  useEffect(() => {
    if (project) localStorageSet('last', project.id);
  }, [project?.id]); // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => {
    const last = localStorageGet('last');
    if (last && !useStore.getState().project) void loadProject(last).then((p) => { if (p && !useStore.getState().project) useStore.getState().open(p); });
  }, []);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const mod = e.metaKey || e.ctrlKey;
      if (leviShell && mod && e.shiftKey && e.key.toLowerCase() === 'f') {
        e.preventDefault();
        setFocus((current) => !current);
        return;
      }
      if (mod && e.key.toLowerCase() === 's') {
        e.preventDefault();
        // ⌘S saves to the project's file (asking where the first time); ⇧⌘S is Save As.
        void saveToFile(e.shiftKey);
      }
      // Project-level undo for non-script edits (script undo is handled inside the editor).
      const inEditor = (e.target as HTMLElement)?.closest?.('.ProseMirror, input, textarea, select');
      if (mod && !inEditor && e.key.toLowerCase() === 'z') {
        e.preventDefault();
        if (e.shiftKey) useStore.getState().redo();
        else useStore.getState().undo();
      }
    };
    const beforeUnload = () => { session.flush(); void flushSave(); };
    window.addEventListener('keydown', onKey);
    window.addEventListener('beforeunload', beforeUnload);
    return () => { window.removeEventListener('keydown', onKey); window.removeEventListener('beforeunload', beforeUnload); };
  }, []);

  if (!project) return <><JoinHandler /><HomeScreen /></>;

  // Per-tab access: tabs hidden by this person's link aren't shown at all.
  const allowed = hiddenSync?.length ? SECTIONS.map((x) => ({ ...x, tabs: x.tabs.filter((t) => !tabHidden(t.id)) })).filter((x) => x.tabs.length) : SECTIONS;
  const fullSec = allowed.find((x) => x.id === section) ?? allowed[0];
  // Open Gate Lite: on phones, only the chosen tabs (a hidden one opened from "Show all tabs" still shows its section).
  const shown = visibleSections(allowed, phoneTabs, phone);
  const sec = shown.find((x) => x.id === section && x.tabs.some((t) => t.id === tab)) ?? fullSec;
  return (
    <div className={`app ${leviShell ? 'levi-shell' : ''} ${focus ? 'levi-focus' : ''} ${phone ? 'is-phone' : ''}`}>
      <CategoryStyles />
      <DialogHost />
      <ActionSheetHost />
      <PinterestReceiver onOpen={openBoards} />
      <JoinHandler />
      {narrow && !leviShell ? (
        <PhoneTopbar sections={shown} allSections={SECTIONS} tab={tab} setTab={setTab as never} saveState={saveState} />
      ) : (
        <header className="topbar">
          <ProjectMenu tab={tab} view={{ sidebarCollapsed: leviShell ? !toolsOpen : sidebarCollapsed, toggleSidebar: leviShell ? toggleTools : toggleSidebar, sidePanel: SIDE_TABS.has(tab) ? { hidden: sideHidden, toggle: toggleSide } : undefined }} />
          <div className="topbar-right">
            {leviShell && <select aria-label="Production stage" value={stage} onChange={(event) => selectStage(parseStage(event.target.value))}>
              {stages.map((entry) => <option key={entry.id} value={entry.id}>{entry.label}</option>)}
            </select>}
            {leviShell && <button type="button" aria-pressed={focus} title="Focus Mode (Ctrl+Shift+F)" onClick={() => setFocus(!focus)}>Focus Mode</button>}
            {leviShell && SIDE_TABS.has(tab) && <button type="button" aria-pressed={!sideHidden} onClick={toggleSide}>{tab === 'screenplay' ? 'Scene navigator' : 'Side panel'}</button>}
            {leviShell && (tab === 'screenplay' || tab === 'shots') && <details className="workspace-layout">
              <summary>Layout</summary>
              <div className="workspace-layout-controls">
                <label>{tab === 'screenplay' ? 'Navigator width' : 'Inspector width'}
                  <input type="range" min={180} max={420} step={10} value={panelWidth}
                    onChange={(event) => resizePanel(event.target.valueAsNumber)} />
                  <output>{panelWidth} px</output>
                </label>
                <button type="button" onClick={resetLayout}>Reset Layout</button>
              </div>
            </details>}
            {leviShell && <button type="button" aria-expanded={toolsOpen} onClick={toggleTools}>Tools</button>}
            {leviShell && <>
              <button type="button" onClick={() => void saveToFile()} disabled={saveState === 'saving' || file.writing}>Save</button>
              <button type="button" onClick={() => void saveToFile(true)} disabled={file.writing} title="Export a portable .opengate file">Export project…</button>
            </>}
            <UndoRedo />
            <ShareBar />
            <SyncIssues onGo={setTab} />
            <span className={`save-state ${saveState}`} title={file.name ? `Saved in this browser${file.savedAt ? ` · last written to ${file.name} at ${new Date(file.savedAt).toLocaleTimeString()}` : ''}${file.autosave ? ' · autosaving to the file' : ''}` : leviShell ? 'Autosaved in this app on this device. Export project creates a portable .opengate copy.' : 'Saved in this browser. ⌘S saves a file too.'}>
              {saveState === 'saved' ? (leviShell ? 'Saved locally' : 'Saved') : saveState === 'saving' ? 'Saving…' : saveState === 'failed' ? 'Save Failed' : 'Unsaved'}{file.name && file.autosave && !file.needsPermission ? ' · file' : ''}
            </span>
          </div>
        </header>
      )}
      <div className="app-body">
      {(!leviShell ? !narrow : toolsOpen && !focus) && <ProjectSidebar sections={allowed} tab={tab} setTab={setTab as (t: string) => void} firstTabIn={lastTabIn as never} collapsed={leviShell ? false : sidebarCollapsed} onToggle={leviShell ? toggleTools : toggleSidebar} />}
      <div className="app-main" style={leviShell ? { '--workspace-panel-width': panelWidth + 'px' } as React.CSSProperties : undefined}>
      {narrow && !(leviShell && isPlan) && (sec.tabs.length > 1 || (!phone && SIDE_TABS.has(tab))) && (
        <nav className="subtabs" aria-label={`${sec.label} tabs`}>
          {!phone && SIDE_TABS.has(tab) && (
            <button className={`subtab side-toggle ${sideHidden ? 'on' : ''}`} title={`${sideHidden ? 'Show' : 'Hide'} the side panel (${navigator.platform.includes('Mac') ? '⌘' : 'Ctrl+'}\\)`} aria-pressed={sideHidden} onClick={toggleSide}>{sideHidden ? '⇥' : '⇤'}</button>
          )}
          {sec.tabs.length === 1 && <span className="subtab-title">{sec.tabs[0].label}</span>}
          {sec.tabs.length > 1 && sec.tabs.map((t) => (
            <button key={t.id} className={tab === t.id ? 'subtab active' : 'subtab'} title={t.hint} onClick={() => setTab(t.id)}>{t.label}</button>
          ))}
        </nav>
      )}
      <AccessBanner />
      <HelpDialogs />
      <Tour />
      <TourOffer />
      {phone && <PhonePanels tab={tab} />}
      {persistentWrite && <main hidden={!isWrite} className={`tab-body ${(focus || hiddenSides.includes('screenplay')) && !phone ? 'side-hidden' : ''}`}>
        <TabBoundary tab="screenplay"><WriteTab key={project.id} onOpenPlan={canPlan ? openPlan : undefined} /></TabBoundary>
      </main>}
      {(!persistentWrite || !isWrite) && <main className={`tab-body ${sideHidden && !phone ? 'side-hidden' : ''}`}>
        <TabBoundary tab={tab}>
        <Suspense fallback={<p className="pad muted" role="status">Loading workspace…</p>}>
        {tab === 'screenplay' && <WriteTab onOpenPlan={leviShell && canPlan ? openPlan : undefined} />}
        {tab === 'av' && <AVScriptTab />}
        {tab === 'docs' && <DocsTab />}
        {tab === 'breakdown' && <BreakdownTab />}
        {tab === 'stripboard' && <ScheduleTab />}
        {tab === 'dood' && <DoodView />}
        {tab === 'sides' && <SidesView />}
        {['budget', 'funding', 'festivals', 'gear', 'expenses', 'licenses'].includes(tab) && <BudgetCapBar />}
        {['contacts', 'locations', 'paperwork', 'calendar', 'tasks'].includes(tab) && <PlanBudgetSummary />}
        {tab === 'budget' && <BudgetTab />}
        {tab === 'expenses' && <ExpensesTab />}
        {tab === 'gear' && <GearTab />}
        {tab === 'funding' && <FundingTab />}
        {tab === 'festivals' && <FestivalsTab />}
        {tab === 'presskit' && <PressKitTab />}
        {tab === 'social' && <SocialTab />}
        {tab === 'distribution' && <DistributionTab />}
        {tab === 'licenses' && <LicensesTab />}
        {tab === 'paperwork' && <PaperworkTab />}
        {tab === 'contacts' && <ContactsTab />}
        {tab === 'calendar' && <CalendarTab />}
        {tab === 'tasks' && <TasksTab />}
        {leviShell && isPlan && <PlanWorkspace key={project.id + ':' + layoutReset} layout={planLayout} focus={focus} onLayout={(layout) => {
          rememberPlan(layout);
        }} scene={planScene} onSceneChange={setPlanScene} />}
        {tab === 'storyboard' && <StoryboardTab />}
        {tab === 'boards' && <BoardsTab />}
        {!leviShell && tab === 'shots' && <ShotsTab />}
        {tab === 'designer' && <ShotDesignerTab />}
        {tab === 'callsheets' && <CallSheetsView />}
        {tab === 'locations' && <LocationsTab />}
        {tab === 'characters' && <CharactersTab />}
        {tab === 'bible' && <StoryBibleTab />}
        {tab === 'reports' && <ShootTab />}
        {tab === 'setup' && <SetupTab />}
        {tab === 'reference' && <ReferenceTab />}
        </Suspense>
        </TabBoundary>
      </main>}
      </div>
      </div>
      {leviShell && <nav className="levi-workspaces" aria-label="Workspaces">
        {availableWorkspaces.map((workspace) => <button key={workspace.tab} type="button"
          aria-current={tab === workspace.tab ? 'page' : undefined}
          onClick={() => selectWorkspace(workspace.tab)}>{workspace.label}</button>)}
      </nav>}
    </div>
  );
}

function localStorageGet(k: string): string | null {
  try { return localStorage.getItem('slateline.' + k); } catch { return null; }
}
function localStorageSet(k: string, v: string) {
  try { localStorage.setItem('slateline.' + k, v); } catch { /* ignore */ }
}
