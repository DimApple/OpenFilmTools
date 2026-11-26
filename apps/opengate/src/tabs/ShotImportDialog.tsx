import { useState } from 'react';
import { Field, Modal } from '../components/common';
import { SHOT_COLUMNS } from '../io/reports';
import { SHOT_TYPES } from '../model/defaults';
import type { DerivedScene, Shot } from '../model/types';

type Column = Exclude<(typeof SHOT_COLUMNS)[number][0], '#' | 'id' | 'sceneId' | 'order'>;
type Mapping = Partial<Record<Column, number>>;
type Sheet = { name: string; rows: string[][] };
export type ImportedShot = { sceneId: string | null; fields: Partial<Shot> };

const columns = SHOT_COLUMNS.filter(([key]) => key !== '#') as [Column, string][];
const aliases: Partial<Record<Column, string[]>> = {
  scene: ['Scene Number', 'Scene #', 'Scene Heading', 'Scene No'],
  shotNumber: ['Shot', 'Shot Number', 'Shot ID', 'Shot No'],
  shotType: ['Type', 'Framing', 'Shot Size'],
  lens: ['Focal Length'],
  action: ['Visual Action', 'Shot Action'],
  move: ['Static/Movement', 'Camera Movement', 'Camera Move'],
  setup: ['Setup #', 'Setup Number'],
};
const normalize = (text: string) => text.toLowerCase().replace(/[^a-z0-9]/g, '');
const shotKey = (sceneId: string | null, number: string) => JSON.stringify([sceneId, number.trim().toLowerCase()]);

function guessMapping(row: string[]): Mapping {
  const result: Mapping = {};
  for (const [key, label] of columns) {
    const names = [label, ...(aliases[key] ?? [])].map(normalize);
    const index = row.findIndex((text) => names.includes(normalize(text)));
    if (index >= 0) result[key] = index;
  }
  return result;
}

interface Props {
  scenes: DerivedScene[];
  existing: Shot[];
  sceneOf: (shot: Shot) => string | null;
  onClose: () => void;
  onImport: (shots: ImportedShot[]) => void;
}

export function ShotImportDialog({ scenes, existing, sceneOf, onClose, onImport }: Props) {
  const [sheets, setSheets] = useState<Sheet[]>([]);
  const [sheetIndex, setSheetIndex] = useState(0);
  const [header, setHeader] = useState(0);
  const [mapping, setMapping] = useState<Mapping>({});
  const [overrides, setOverrides] = useState<Record<number, string>>({});
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const rows = sheets[sheetIndex]?.rows ?? [];
  const headings = rows[header] ?? [];
  const sceneIds = new Set(scenes.map((scene) => scene.id));
  const seen = new Set(existing.map((shot) => {
    const id = sceneOf(shot);
    return shotKey(id && sceneIds.has(id) ? id : null, shot.shotNumber);
  }));

  const preview = rows.slice(header + 1).flatMap((row, index) => {
    const fields: Partial<Shot> = {};
    for (const [key] of columns) {
      const column = mapping[key];
      if (key !== 'scene' && column !== undefined) fields[key] = row[column]?.trim() ?? '';
    }
    if (!Object.values(fields).some(Boolean)) return [];
    const rowNumber = header + index + 2;
    const source = row[mapping.scene ?? -1]?.trim() ?? '';
    const matches = scenes.filter((scene) => source && (
      scene.number.toLowerCase() === source.replace(/^scene\s*#?\s*/i, '').toLowerCase()
      || normalize(scene.heading) === normalize(source)
    ));
    const sceneId = (overrides[rowNumber] ?? (matches.length === 1 ? matches[0].id : '')) || null;
    const key = shotKey(sceneId, fields.shotNumber ?? '');
    const duplicate = !!fields.shotNumber && seen.has(key);
    if (fields.shotNumber) seen.add(key);
    if (fields.shotType) {
      const type = normalize(fields.shotType);
      fields.shotType = SHOT_TYPES.find(({ code, label }) => [code, label].map(normalize).includes(type))?.code ?? fields.shotType;
    }
    return [{ rowNumber, source, duplicate, sceneId, fields }];
  });
  const ready = preview.filter((row) => !row.duplicate);

  function selectSheet(sources: Sheet[], index: number) {
    const candidates = sources[index].rows.slice(0, 30).map((row) => Object.keys(guessMapping(row)).length);
    const chosen = candidates.indexOf(Math.max(...candidates));
    setSheetIndex(index);
    setHeader(chosen);
    setMapping(guessMapping(sources[index].rows[chosen]));
    setOverrides({});
  }

  async function load(file: File) {
    setBusy(true);
    setError('');
    setSheets([]);
    try {
      const xlsx = await import('xlsx');
      const book = xlsx.read(await file.arrayBuffer(), { type: 'array', raw: true });
      const sources = book.SheetNames.map((name) => ({
        name,
        rows: xlsx.utils.sheet_to_json<string[]>(book.Sheets[name], { header: 1, defval: '', raw: false, blankrows: true }),
      })).filter((sheet) => sheet.rows.length > 1);
      if (!sources.length) throw new Error('Choose a sheet with a header and at least one shot.');
      setSheets(sources);
      selectSheet(sources, Math.max(0, sources.findIndex((sheet) => /^shot list$/i.test(sheet.name))));
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <Modal title="Import shot list" wide onClose={onClose} footer={
      <button className="primary" disabled={busy || !ready.length} onClick={() => onImport(ready)}>Import {ready.length} shots</button>
    }>
      <Field label="CSV or Excel file">
        <input type="file" accept=".csv,.xlsx,.xls" disabled={busy} onChange={(event) => {
          const file = event.currentTarget.files?.[0];
          event.currentTarget.value = '';
          if (file) void load(file);
        }} />
      </Field>
      {busy && <p role="status">Reading spreadsheet…</p>}
      {error && <p role="alert">{error}</p>}
      {!!sheets.length && <>
        {sheets.length > 1 && <Field label="Worksheet">
          <select value={sheetIndex} onChange={(event) => selectSheet(sheets, Number(event.target.value))}>
            {sheets.map((sheet, index) => <option key={sheet.name} value={index}>{sheet.name}</option>)}
          </select>
        </Field>}
        <Field label="Header row">
          <input type="number" min={1} max={rows.length} value={header + 1} onChange={(event) => {
            const index = Math.max(0, Math.min(rows.length - 1, Number(event.target.value) - 1));
            setHeader(index);
            setMapping(guessMapping(rows[index]));
            setOverrides({});
          }} />
        </Field>
        <details open>
          <summary>Map columns</summary>
          <div className="form-grid">
            {columns.map(([key, label]) => <Field key={key} label={label}>
              <select value={mapping[key] ?? ''} onChange={(event) => {
                setMapping({ ...mapping, [key]: event.target.value === '' ? undefined : Number(event.target.value) });
                if (key === 'scene') setOverrides({});
              }}>
                <option value="">Skip</option>
                {headings.map((heading, index) => <option key={index} value={index}>{index + 1}. {heading || '(blank)'}</option>)}
              </select>
            </Field>)}
          </div>
        </details>
        <p role="status">{ready.length} ready · {preview.length - ready.length} duplicates skipped</p>
        <table className="table">
          <thead><tr><th>Row</th><th>Shot</th><th>Scene</th><th>Action</th><th>Status</th></tr></thead>
          <tbody>{preview.map(({ rowNumber, fields, sceneId, source, duplicate }) => <tr key={rowNumber}>
            <td>{rowNumber}</td><td>{fields.shotNumber || '(auto)'}</td>
            <td><select aria-label={`Scene for row ${rowNumber}`} value={sceneId ?? ''} onChange={(event) => setOverrides({ ...overrides, [rowNumber]: event.target.value })}>
              <option value="">{source ? `Unassigned (${source})` : 'Unassigned'}</option>
              {scenes.map((scene) => <option key={scene.id} value={scene.id}>{scene.number}. {scene.heading}</option>)}
            </select></td>
            <td>{fields.action || fields.name}</td><td>{duplicate ? 'Duplicate' : 'Ready'}</td>
          </tr>)}</tbody>
        </table>
      </>}
    </Modal>
  );
}
