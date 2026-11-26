import { afterEach, expect, it, vi } from 'vitest';
import { newProject } from './model/defaults';
import { saveProject } from './store/db';
import { flushSave, useStore } from './store/project';

vi.mock('./store/db', () => ({ saveProject: vi.fn() }));

afterEach(() => {
  useStore.getState().close();
  vi.resetAllMocks();
});

it('writes snapshots in order and keeps newer edits dirty until written', async () => {
  const save = vi.mocked(saveProject);
  let finishFirst!: () => void;
  save.mockImplementationOnce(() => new Promise<void>((resolve) => { finishFirst = resolve; }));
  save.mockResolvedValueOnce(undefined);
  useStore.getState().open(newProject('First'));
  const first = flushSave();
  await vi.waitFor(() => expect(save).toHaveBeenCalledTimes(1));
  useStore.getState().update((draft) => { draft.title = 'Second'; });
  finishFirst();
  await first;
  expect(useStore.getState().saveState).toBe('dirty');
  await flushSave();
  expect(save.mock.calls.map(([project]) => project.title)).toEqual(['First', 'Second']);
  expect(useStore.getState().saveState).toBe('saved');
});

it('reports a write failure and allows retrying the same snapshot', async () => {
  const save = vi.mocked(saveProject);
  save.mockRejectedValueOnce(new Error('Disk unavailable')).mockResolvedValueOnce(undefined);
  useStore.getState().open(newProject());
  await expect(flushSave()).rejects.toThrow('Disk unavailable');
  expect(useStore.getState().saveState).toBe('failed');
  await flushSave();
  expect(useStore.getState().saveState).toBe('saved');
});
