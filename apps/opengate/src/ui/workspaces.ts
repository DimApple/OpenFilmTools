export const stages = [
  { id: 'pre', label: 'Pre-Production' },
  { id: 'production', label: 'Production' },
  { id: 'post', label: 'Post-Production' },
  { id: 'all', label: 'All Tools' },
] as const;

export type Stage = typeof stages[number]['id'];

export const workspaces = [
  { tab: 'screenplay', label: 'Write', stages: ['pre'] },
  { tab: 'shots', label: 'Plan', stages: ['pre', 'production'] },
  { tab: 'storyboard', label: 'Storyboard', stages: ['pre'] },
  { tab: 'reports', label: 'Deliver', stages: ['pre', 'post'] },
] as const satisfies readonly { tab: string; label: string; stages: readonly Stage[] }[];

export function parseStage(value: string | null): Stage {
  return stages.find((stage) => stage.id === value)?.id ?? 'pre';
}

export function workspacesFor(stage: Stage) {
  return workspaces.filter((workspace) =>
    stage === 'all' || workspace.stages.some((membership) => membership === stage));
}
