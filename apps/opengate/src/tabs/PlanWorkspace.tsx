import { lazy, Suspense, useRef, useState } from 'react';
import { Field } from '../components/common';
import { boardShots, insertFrame, reassignFrame, reorderFrame } from '../script/storyboard';
import { useDerived, useProject, useStore } from '../store/project';
import { canWrite, tabHidden } from '../sync/state';
import { useNarrow } from '../ui/responsive';
import { ShotsTab } from './ShotsTab';
const StoryboardTab = lazy(() => import('./StoryboardTab').then(({ StoryboardTab }) => ({ default: StoryboardTab })));

export type PlanLayout = 'shots' | 'storyboard' | 'split';

interface Props {
  layout: PlanLayout;
  focus?: boolean;
  onLayout: (layout: PlanLayout) => void;
  scene: string;
  onSceneChange: (scene: string) => void;
}

const views: { id: PlanLayout; label: string }[] = [
  { id: 'shots', label: 'Shot List' },
  { id: 'storyboard', label: 'Storyboard' },
  { id: 'split', label: 'Split' },
];

export function PlanWorkspace({ layout, focus, onLayout, scene, onSceneChange }: Props) {
  const project = useProject();
  const derived = useDerived();
  const update = useStore((state) => state.update);
  const narrow = useNarrow();
  const [inspector, setInspector] = useState(() => {
    try {
      const saved = localStorage.getItem('slateline.planInspector');
      return saved === 'open' || (saved !== 'closed' && !narrow);
    } catch { return !narrow; }
  });
  const [selection, setSelection] = useState<{ shotId: string; frameId?: string }>();
  const root = useRef<HTMLDivElement>(null);
  const shotsAllowed = !tabHidden('shots');
  const boardAllowed = !tabHidden('storyboard');
  const active = shotsAllowed && boardAllowed ? layout : shotsAllowed ? 'shots' : 'storyboard';
  const sceneFilter = scene === 'none' || derived.sceneById.has(scene) ? scene : '';
  const shots = boardShots(project, derived);
  const frame = project.storyboard.find((entry) => entry.id === selection?.frameId);
  const shotId = frame?.shotId ?? selection?.shotId;
  const selected = shots.find((entry) => entry.shot.id === shotId);
  const visibleSelection = selected && (!sceneFilter || (selected.scene?.id ?? 'none') === sceneFilter) ? selected : undefined;
  const selectedShotId = visibleSelection?.shot.id;
  const panelIndex = visibleSelection?.frames.findIndex((entry) => entry.id === frame?.id) ?? -1;
  const canEditPanels = boardAllowed && canWrite('storyboard');

  function select(shotId: string, frameId?: string, scrollTo?: 'shot' | 'panel') {
    setSelection({ shotId, frameId });
    if (active !== 'split' || !scrollTo) return;
    root.current?.querySelector(`[data-plan-${scrollTo}="${CSS.escape(shotId)}"]`)?.scrollIntoView({ block: 'nearest', inline: 'nearest' });
  }

  function movePanel(targetId: string) {
    const target = shots.find((entry) => entry.shot.id === targetId);
    if (!canEditPanels || !frame || !target || targetId === frame.shotId) return;
    update((draft) => reassignFrame(draft, frame.id, targetId), 'Move storyboard panel to shot');
    if (sceneFilter) onSceneChange(target.scene?.id ?? 'none');
    setSelection({ shotId: targetId, frameId: frame.id });
  }

  function reorderPanel(direction: -1 | 1) {
    if (!canEditPanels || !frame || !visibleSelection || panelIndex < 0) return;
    const neighbor = visibleSelection.frames[panelIndex + direction];
    if (!neighbor) return;
    update((draft) => reorderFrame(draft, frame.id, direction), 'Reorder storyboard panels');
  }

  function addPanel(id: string) {
    if (!canEditPanels) return;
    let frameId: string | undefined;
    update((draft) => { frameId = insertFrame(draft, id); }, 'Add storyboard panel');
    if (frameId) setSelection({ shotId: id, frameId });
  }

  return (
    <div ref={root} className="plan-workspace">
      <div className="levi-plan-tabs">
        <nav aria-label="Plan views">
          {views.filter(({ id }) => id === 'split' ? shotsAllowed && boardAllowed : id === 'shots' ? shotsAllowed : boardAllowed)
            .map(({ id, label }) => <button key={id} type="button" aria-current={active === id ? 'page' : undefined} onClick={() => onLayout(id)}>{label}</button>)}
        </nav>
        <label className="plan-scene-filter"><span>Scene</span><select aria-label="Plan scene" value={sceneFilter} onChange={(event) => { setSelection(undefined); onSceneChange(event.target.value); }}>
          <option value="">All scenes</option>
          {derived.scenes.map((entry) => <option key={entry.id} value={entry.id}>{entry.number}. {entry.heading}</option>)}
          <option value="none">No scene</option>
        </select></label>
        {!focus && <button type="button" className="plan-inspector-toggle" aria-expanded={inspector} onClick={() => {
          setInspector(!inspector);
          try { localStorage.setItem('slateline.planInspector', inspector ? 'closed' : 'open'); } catch { /* storage unavailable */ }
        }}>Inspector</button>}
      </div>
      <div className="plan-body">
        <div className={`plan-panes ${active === 'split' ? 'plan-panes-split' : ''}`}>
          {active !== 'storyboard' && <section className="plan-pane" aria-label="Shot list">
            <ShotsTab scene={sceneFilter} split={active === 'split'} selectedShotId={selectedShotId} onSelectShot={(id) => select(id, undefined, 'panel')} />
          </section>}
          {active !== 'shots' && <section className="plan-pane" aria-label="Storyboard">
            <Suspense fallback={<p className="pad muted" role="status">Loading storyboard…</p>}>
              <StoryboardTab scene={sceneFilter} selectedShotId={selectedShotId} selectedFrameId={panelIndex >= 0 ? frame?.id : undefined} onSelectPanel={(id, panelId) => select(id, panelId, 'shot')} />
            </Suspense>
          </section>}
        </div>
        {inspector && !focus && <aside className="plan-inspector" aria-label="Plan inspector">
          {!visibleSelection ? <p className="muted">Select a shot or storyboard panel.</p> : <>
            <h3>{visibleSelection.shot.shotNumber ? `Shot ${visibleSelection.shot.shotNumber}` : 'Shot'}</h3>
            {visibleSelection.scene && <p className="muted small">{visibleSelection.scene.number}. {visibleSelection.scene.heading}</p>}
            <p>{visibleSelection.shot.action || visibleSelection.shot.name}</p>
            {(visibleSelection.shot.shotType || visibleSelection.shot.lens || visibleSelection.shot.move) && <p className="small">{[visibleSelection.shot.shotType, visibleSelection.shot.lens, visibleSelection.shot.move].filter(Boolean).join(' · ')}</p>}
            {boardAllowed && <>
              <Field label="Storyboard panel">
                <select value={panelIndex >= 0 ? frame?.id : ''} onChange={(event) => select(visibleSelection.shot.id, event.target.value || undefined)}>
                  <option value="">Shot overview</option>
                  {visibleSelection.frames.map((entry, index) => <option key={entry.id} value={entry.id}>Panel {index + 1}</option>)}
                </select>
              </Field>
              <button type="button" disabled={!canEditPanels} onClick={() => addPanel(visibleSelection.shot.id)}>Add panel to shot</button>
            </>}
            {frame && panelIndex >= 0 && boardAllowed && <>
              <Field label="Panel caption" wide>
                <textarea rows={3} disabled={!canEditPanels} value={frame.caption ?? ''} onChange={(event) => {
                  const caption = event.target.value;
                  update((draft) => {
                    const panel = draft.storyboard.find((entry) => entry.id === frame.id);
                    if (panel) panel.caption = caption;
                  }, null);
                }} />
              </Field>
              <div className="plan-panel-order">
                <button type="button" disabled={!canEditPanels || panelIndex === 0} onClick={() => reorderPanel(-1)}>Earlier</button>
                <button type="button" disabled={!canEditPanels || panelIndex === visibleSelection.frames.length - 1} onClick={() => reorderPanel(1)}>Later</button>
              </div>
              <Field label="Move panel to shot">
                <select value="" disabled={!canEditPanels || shots.length < 2} onChange={(event) => movePanel(event.target.value)}>
                  <option value="">Choose a shot…</option>
                  {shots.filter((entry) => entry.shot.id !== frame.shotId).map(({ shot, scene }) => <option key={shot.id} value={shot.id}>
                    {[scene?.number ? `Scene ${scene.number}` : 'No scene', shot.shotNumber || 'Unnumbered shot', shot.name || shot.action].filter(Boolean).join(' · ')}
                  </option>)}
                </select>
              </Field>
            </>}
          </>}
        </aside>}
      </div>
    </div>
  );
}
