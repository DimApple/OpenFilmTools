import 'fake-indexeddb/auto';
import { describe, expect, it } from 'vitest';
import { exportProjectFile, readProjectFile } from './io/files';
import { newProject } from './model/defaults';
import { screenplayPdf } from './pdf/screenplayPdf';
import { coverageLanes, coverageRanges } from './script/coverage';
import { derive } from './script/derive';
import { schema } from './script/schema';
import { blankShot } from './tabs/ShotsTab';

describe('lined screenplay', () => {
  it('preserves overlapping shot IDs and repeats the continued label on a second page', async () => {
    const a = schema.marks.shotRef.create({ shotId: 'a' });
    const b = schema.marks.shotRef.create({ shotId: 'b' });
    const doc = schema.node('doc', null, [
      schema.node('sceneHeading', { sceneId: 'scene', number: null }, [schema.text('INT. ROOM - DAY')]),
      schema.node('action', null, [
        schema.text('Long text '.repeat(410), [a]),
        schema.text('Overlapping text '.repeat(14), [a, b]),
        schema.text('B only', [b]),
      ]),
    ]);
    const ranges = coverageRanges(doc);
    expect(ranges.map((range) => range.shotId)).toEqual(['a', 'b']);
    expect(ranges[1].from).toBeLessThan(ranges[0].to);
    expect(coverageLanes(ranges.map(({ from, to }) => ({ start: from, end: to }))).map(({ lane }) => lane)).toEqual([0, 1]);

    const project = newProject();
    project.scriptDoc = doc.toJSON();
    project.shots = [
      { ...blankShot('scene', 0, 'CAMERA-A'), id: 'a' },
      { ...blankShot('scene', 1, 'CAMERA-B'), id: 'b' },
    ];
    project.storyboard = [{ id: 'frame', shotId: 'a', order: 0, imageId: 'image', rev: 0, caption: '' }];
    const image = new Blob([new Uint8Array([1, 2, 3])], { type: 'image/png' });
    const bundle = await exportProjectFile(project, async (id) => id === 'image' ? image : null);
    const importedMedia: string[] = [];
    const reopened = await readProjectFile(new File([bundle], 'levi.opengate'), false, async (_pid, blob, id) => {
      expect(blob.size).toBe(3);
      expect(blob.type).toBe('image/png');
      importedMedia.push(id);
    });
    expect(reopened.id).toBe(project.id);
    expect(reopened.shots.map((shot) => shot.id)).toEqual(['a', 'b']);
    expect(reopened.storyboard[0]).toMatchObject({ id: 'frame', shotId: 'a', imageId: 'image' });
    expect(importedMedia).toEqual(['image']);
    expect(coverageRanges(schema.nodeFromJSON(reopened.scriptDoc))).toEqual(ranges);

    const derived = derive(project.scriptDoc);
    const lined = screenplayPdf(project, derived, { lined: true, titlePage: false });
    expect(lined.getNumberOfPages()).toBeGreaterThan(1);
    expect(lined.output().match(/\(CAMERA-A\)/g)?.length).toBe(lined.getNumberOfPages());
    expect(lined.output()).toContain('(CAMERA-B)');
    expect(screenplayPdf(project, derived, { titlePage: false }).output()).not.toContain('(CAMERA-A)');
  });
});
