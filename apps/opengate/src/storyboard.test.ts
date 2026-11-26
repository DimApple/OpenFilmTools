import { describe, expect, it, beforeEach } from 'vitest';
import 'fake-indexeddb/auto';
import { produce } from 'immer';
import { newProject } from './model/defaults';
import { useStore } from './store/project';
import { session } from './script/session';
import { addSceneToScript } from './script/scenes';
import { derive } from './script/derive';
import { storyboardPanels, shotTypeCodes, reassignFrame, reorderFrame } from './script/storyboard';
import { storyboardPdf } from './pdf/storyboardPdf';
import { blankShot } from './tabs/ShotsTab';

beforeEach(() => {
  session.reset();
  useStore.getState().open(newProject('Board first'));
});

describe('starting anywhere', () => {
  it('adds scenes to an empty script from other tabs', () => {
    const a = addSceneToScript('ext. rooftop - night');
    const b = addSceneToScript('INT. STAIRWELL - CONTINUOUS', 'Footsteps.');
    const p = useStore.getState().project!;
    const d = derive(p.scriptDoc);
    expect(d.scenes.map((s) => [s.id, s.heading])).toEqual([[a, 'EXT. ROOFTOP - NIGHT'], [b, 'INT. STAIRWELL - CONTINUOUS']]);
    expect(d.scenes[1].firstAction).toBe('Footsteps.');
  });
});

describe('storyboard panels', () => {
  it('shows every shot, with or without frames, in scene order', () => {
    const sc = addSceneToScript('INT. DINER - DAY');
    let p = useStore.getState().project!;
    p = produce(p, (dr) => {
      dr.shots.push({ ...blankShot(sc, 1, '1B'), id: 's2' }, { ...blankShot(sc, 0, '1A'), id: 's1', shotType: 'CU/MS' }, { ...blankShot(null, 0, 'X'), id: 's3' });
      dr.storyboard.push({ id: 'f1', shotId: 's1', order: 0, rev: 0, caption: '' }, { id: 'f2', shotId: 's1', order: 1, rev: 0, caption: '' });
    });
    const panels = storyboardPanels(p, derive(p.scriptDoc));
    expect(panels.map((x) => [x.shot.id, x.frame?.id ?? null, `${x.frameNo}/${x.frameCount}`])).toEqual([
      ['s1', 'f1', '1/2'], ['s1', 'f2', '2/2'], ['s2', null, '1/1'], ['s3', null, '1/1'],
    ]);
    expect(shotTypeCodes(panels[0].shot.shotType)).toEqual(['CU', 'MS']);
    for (const perPage of [1, 2, 3, 4, 6] as const) {
      const doc = storyboardPdf(p, derive(p.scriptDoc), new Map(), { perPage, showReference: true });
      expect(doc.getNumberOfPages()).toBe(Math.ceil(panels.length / perPage));
    }
    const text = storyboardPdf(p, derive(p.scriptDoc), new Map(), { perPage: 2, showReference: true }).output();
    expect(text).toContain('SET PLAN');
    expect(text).toContain('ELS');
  });
});

it('reorders tied panel positions and reassigns a frame without changing its media identity', () => {
  const project = newProject('Storyboard reassignment');
  const source = blankShot(null, 0, '1A');
  const target = blankShot(null, 1, '1B');
  project.shots.push(source, target);
  project.storyboard.push(
    { id: 'a', shotId: source.id, order: 0, rev: 2, imageId: 'plate', drawingId: 'ink', caption: 'Opening' },
    { id: 'b', shotId: source.id, order: 0, rev: 0, caption: '' },
    { id: 'c', shotId: target.id, order: 10, rev: 0, caption: '' },
  );
  reorderFrame(project, 'b', -1);
  const derived = derive(project.scriptDoc);
  expect(storyboardPanels(project, derived).map(({ frame }) => frame?.id)).toEqual(['b', 'a', 'c']);
  reassignFrame(project, 'a', target.id);
  expect(storyboardPanels(project, derived).map(({ frame }) => frame?.id)).toEqual(['b', 'c', 'a']);
  expect(project.storyboard.find(({ id }) => id === 'a')).toEqual({
    id: 'a', shotId: target.id, order: 11, rev: 2, imageId: 'plate', drawingId: 'ink', caption: 'Opening',
  });
});
