export type SourceFile = { path: string; mediaType: string; content: string };
export type Package = { name: string; description: string; languageId: string; runtimeId: string; entryFile: string; entrySymbol: string; contractId: string; manifest: object; files: SourceFile[] };
export type Draft = Package & { draftId: string; draftVersion: string; processorId?: string; baseVersion?: string; updatedAt?: string };
export type DraftSummary = Omit<Draft, 'files' | 'manifest'> & { fileCount: number };
export type Diagnostic = { message: string; file?: string; line?: number; column?: number; code: string };
export type SavedProcessor = { id: string; version: string; package: Package };
export type ProcessorSummary = { id: string; name: string; description: string; languageId: string; version: string };

export function hasEditingPermission(connected: boolean, token: string, deadline: number, now: number, leaving: boolean): boolean {
  return connected && Boolean(token) && now < deadline && !leaving;
}

export function applyEditingLock(connected: boolean, editor: { updateOptions(options: { readOnly: boolean }): void },
  fields: { readOnly: boolean }[], controls: { disabled: boolean }[]): void {
  editor.updateOptions({ readOnly: !connected });
  for (const field of fields) field.readOnly = !connected;
  for (const control of controls) control.disabled = !connected;
}

export type EditorTarget = { processorId?: string; languageId?: string };
export type DraftApi = (path: string, method?: string, body?: unknown) => Promise<Draft | SavedProcessor>;

export async function openDraftContent(api: DraftApi, target: EditorTarget, template: Package, persist = false): Promise<Draft> {
  if (persist) return await api('/drafts', 'POST', target.processorId
    ? { processorId: target.processorId, languageId: target.languageId }
    : template) as Draft;
  if (target.processorId) {
    const current = await api(`/processors/${encodeURIComponent(target.processorId)}`) as SavedProcessor;
    return { ...structuredClone(current.package), processorId: current.id, baseVersion: current.version, draftId: '', draftVersion: '' };
  }
  return { ...structuredClone(template), processorId: '', baseVersion: '', draftId: '', draftVersion: '', updatedAt: undefined };
}

export function needsDraftWrite(local: Draft, acknowledged: Draft): boolean {
  return Boolean(local.draftId) || !sameContent(local, acknowledged);
}

export function formatDraftTime(value: string | undefined, locale: string): string {
  if (!value) return '';
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? '' : new Intl.DateTimeFormat(locale, { dateStyle: 'medium', timeStyle: 'short' }).format(date);
}

export function isMissingEntry(item: Diagnostic, entrySymbol: string): boolean {
  return item.code === 'invalid_source' && ['Lua', 'JavaScript'].some(language =>
    item.message === `${language} processor must define function ${entrySymbol}(context).`);
}

export function chooseRecoveryDraft(drafts: Pick<Draft, 'draftId'>[], preferredId?: string): string | undefined {
  if (preferredId && drafts.some(draft => draft.draftId === preferredId)) return preferredId;
  return drafts.length === 1 ? drafts[0].draftId : undefined;
}

export function filterProcessors(processors: ProcessorSummary[], query: string): ProcessorSummary[] {
  const search = query.trim().normalize('NFKC').toLocaleLowerCase();
  return processors.filter(item => [item.name, item.description, item.languageId]
    .some(value => value.normalize('NFKC').toLocaleLowerCase().includes(search)))
    .sort((a, b) => a.name.localeCompare(b.name) || a.id.localeCompare(b.id));
}

export class CompositionState {
  private readonly owners = new Set<string>();
  get active(): boolean { return this.owners.size > 0; }
  begin(owner: string): void { this.owners.add(owner); }
  end(owner: string): void { this.owners.delete(owner); }
}

function canonical(value: unknown): unknown {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') return Object.fromEntries(Object.entries(value).sort(([a], [b]) => a.localeCompare(b)).map(([key, item]) => [key, canonical(item)]));
  return value;
}
export function sameContent(left: Package, right: Package): boolean {
  const normalize = (value: Package) => canonical({ name: value.name.trim(), description: value.description.trim(), languageId: value.languageId,
    runtimeId: value.runtimeId, entryFile: value.entryFile, entrySymbol: value.entrySymbol, contractId: value.contractId,
    manifest: value.manifest, files: value.files.map(({ path, mediaType, content }) => ({ path, mediaType, content })).sort((a, b) => a.path.localeCompare(b.path)) });
  return JSON.stringify(normalize(left)) === JSON.stringify(normalize(right));
}
export function afterSave(local: Draft, acknowledged: Draft, remote: Draft | undefined, saved: SavedProcessor): 'clear' | 'reuse' | 'branch' {
  if (sameContent(local, saved.package)) return 'clear';
  if (remote?.draftId === local.draftId && remote.processorId === saved.id && remote.baseVersion === saved.version && sameContent(remote, acknowledged)) return 'reuse';
  return 'branch';
}
