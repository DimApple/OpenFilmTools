import type { Node } from 'prosemirror-model';

export interface Coverage {
  shotId: string;
  from: number;
  to: number;
}

export function coverageRanges(doc: Node): Coverage[] {
  const ranges: Coverage[] = [];
  const active = new Map<string, Coverage>();
  doc.descendants((node, pos) => {
    if (node.type.name === 'sceneHeading' || node.type.name === 'pageBreak') active.clear();
    if (!node.isInline) return;
    const ids = new Set<string>(node.marks.filter((mark) => mark.type.name === 'shotRef').map((mark) => mark.attrs.shotId));
    for (const id of active.keys()) if (!ids.has(id)) active.delete(id);
    for (const shotId of ids) {
      const to = pos + node.nodeSize;
      const previous = active.get(shotId);
      if (previous) previous.to = to;
      else {
        const range = { shotId, from: pos, to };
        active.set(shotId, range);
        ranges.push(range);
      }
    }
  });
  return ranges;
}

export function coverageLanes<T extends { start: number; end: number }>(ranges: T[]): (T & { lane: number })[] {
  const ends: number[] = [];
  return [...ranges].sort((a, b) => a.start - b.start || b.end - a.end).map((range) => {
    const free = ends.findIndex((end) => end < range.start);
    const lane = free === -1 ? ends.length : free;
    ends[lane] = range.end;
    return { ...range, lane };
  });
}
