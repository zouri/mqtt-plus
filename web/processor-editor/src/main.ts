import * as monaco from 'monaco-editor/esm/vs/editor/editor.api';
import 'monaco-editor/esm/vs/editor/editor.all';
import 'monaco-editor/esm/vs/basic-languages/lua/lua.contribution';
import 'monaco-editor/esm/vs/basic-languages/javascript/javascript.contribution';
import 'monaco-editor/esm/vs/language/typescript/monaco.contribution';
import editorWorker from 'monaco-editor/esm/vs/editor/editor.worker?worker';
import tsWorker from 'monaco-editor/esm/vs/language/typescript/ts.worker?worker';
import { createIcons, Upload, Download, CheckCheck, Save, FilePlus, Pencil, Trash2, History, X, FolderOpen, ChevronDown, PanelLeft, Settings2, WrapText } from 'lucide';
import { afterSave, chooseRecoveryDraft, CompositionState, filterProcessors, formatDraftTime, isMissingEntry, openDraftContent, needsDraftWrite, sameContent, applyEditingLock, hasEditingPermission, type Diagnostic, type DraftSummary, type ProcessorSummary, type SourceFile, type Package, type Draft } from './draftstate';
import './style.css';

self.MonacoEnvironment = { getWorker: (_id, label) => label === 'javascript' || label === 'typescript' ? new tsWorker() : new editorWorker() };
type Job = { state: string; status?: number; validation?: { ok: boolean; diagnostics: Diagnostic[] }; result?: { error?: string; warnings?: string[]; draft?: Draft | null; processor?: { id: string; version: string; package: Package } } };
class ApiError extends Error { constructor(public status: number, message: string) { super(message); } }
let token = sessionStorage.getItem('token') || '';
let zh = navigator.language.startsWith('zh');
const t = (en: string, cn: string) => zh ? cn : en;
let draft: Draft;
let selected = '';
let generation = 0;
let persistedGeneration = 0;
let pendingJob = false;
let finalizing = false;
let activeJob = '';
let navigating = false;
let dialogLoading = false;
let importing = false;
let acknowledged: Draft;
const compositions = new CompositionState();
let debounce: ReturnType<typeof setTimeout> | undefined;
let maxWait: ReturnType<typeof setTimeout> | undefined;
let writing: Promise<void> = Promise.resolve();
let editor: monaco.editor.IStandaloneCodeEditor;
const models = new Map<string, monaco.editor.ITextModel>();
let templates: Package[] = [];
let theme = 'system';
let draftHistory = readDraftHistory();
let connected = true;
const windowId = crypto.randomUUID();
let leaseToken = '';
let leaseDeadline = 0;
let leaseTimer: ReturnType<typeof setTimeout> | undefined;
let checkingLease = false;
let pageLeaving = false;
let leaseBlockedByOther = false;
let connectionEpoch = 0;
let connectionMessage = '';
let validationState: 'none' | 'running' | 'passed' | 'failed' | 'stale' = 'none';
let validationGeneration = -1;

function readDraftHistory(): Map<string, string> {
  try {
    const entries: unknown = JSON.parse(sessionStorage.getItem('draftHistory') || '[]');
    return new Map(Array.isArray(entries) ? entries.filter((entry): entry is [string, string] => Array.isArray(entry)
      && entry.length === 2 && typeof entry[0] === 'string' && typeof entry[1] === 'string') : []);
  } catch { return new Map(); }
}

async function api<T>(path: string, method = 'GET', body?: unknown, version?: string): Promise<T> {
  const epoch = connectionEpoch;
  let response: Response;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 15000);
  try {
    response = await fetch(`/api/v1${path}`, { signal: controller.signal, method, headers: { Authorization: `Bearer ${token}`, 'X-Editor-Window': windowId, 'X-Editor-Lease': leaseToken, ...(body ? { 'Content-Type': 'application/json' } : {}), ...(version ? { 'If-Match': `"${version}"` } : {}) }, body: body ? JSON.stringify(body) : undefined });
  } catch {
    setConnection(false, t('Program disconnected. The editor is read-only. Export your draft before closing this tab.', '程序已断开，编辑器已锁定为只读。关闭标签页前请导出草稿。'));
    throw new ApiError(0, t('Program disconnected. Unsynced text remains in this tab.', '程序已断开，未同步内容保留在当前标签页。'));
  }
  finally { clearTimeout(timeout); }
  const data = await response.json();
  if (response.status === 423) setLease('', 0);
  if (response.status === 401) setConnection(false, t('Authorization expired. The editor is read-only. Export your draft, then reopen the editor from the app.', '授权已失效，编辑器已锁定为只读。请先导出草稿，再从程序重新打开编辑器。'));
  else if (response.ok && epoch === connectionEpoch) setConnection(true);
  if (!response.ok) throw new ApiError(response.status, data.error || t('Request failed.', '请求失败。'));
  return data as T;
}
function element<T extends HTMLElement>(id: string): T { return document.getElementById(id) as T; }
function status(text: string, error = false) { element('status').textContent = text; element('status').classList.toggle('error', error); }
function setConnection(value: boolean, message = '') {
  if (!value) { ++connectionEpoch; leaseToken = ''; leaseDeadline = 0; clearTimeout(leaseTimer); clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined; }
  const restored = value && !connected;
  connected = value; connectionMessage = message;
  if (!document.getElementById('connection-banner')) return;
  renderConnection();
  if (restored) status(t('Connection restored', '连接已恢复'));
  updateControls(); updateFeedback();
}
function canEdit() { return hasEditingPermission(connected, leaseToken, leaseDeadline, performance.now(), pageLeaving); }
function renderConnection() {
  element('connection-banner').hidden = canEdit();
  element('connection-message').textContent = !connected ? connectionMessage
    : leaseBlockedByOther ? t('Another window is editing. This window is read-only; editing resumes when that window releases it.', '编辑器已在其他窗口打开，本窗口只读。关闭该窗口后将自动恢复编辑。')
    : t('Confirming editing permission. This window is read-only for now.', '正在确认编辑权限，本窗口暂为只读。');
}
function setLease(value: string, deadline: number, blockedByOther = false) {
  leaseBlockedByOther = blockedByOther;
  leaseToken = value; leaseDeadline = deadline; clearTimeout(leaseTimer);
  if (value) leaseTimer = setTimeout(() => setLease('', 0), Math.max(0, deadline - performance.now()));
  else { clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined; }
  if (document.getElementById('connection-banner')) { renderConnection(); updateControls(); updateFeedback(); }
}
async function refreshLease() {
  if (checkingLease || pageLeaving) return;
  if (performance.now() >= leaseDeadline && leaseToken) setLease('', 0);
  checkingLease = true;
  const start = performance.now(), epoch = connectionEpoch;
  const previouslyEditable = canEdit();
  try {
    const lease = await api<{ editable: boolean; leaseToken?: string; leaseMs: number }>('/editor/lease', 'POST', { windowId });
    if (epoch !== connectionEpoch || pageLeaving) return;
    // Lock before the server deadline, even if a renewal stalls or the tab is suspended.
    const deadline = start + lease.leaseMs - 3000;
    setLease(lease.editable && deadline > performance.now() ? lease.leaseToken || '' : '', deadline, !lease.editable);
    if (!previouslyEditable && canEdit() && draft && generation !== persistedGeneration) await flush();
  } finally { checkingLease = false; }
}
function releaseLease() {
  const current = leaseToken;
  pageLeaving = true; setLease('', 0);
  if (current) fetch('/api/v1/editor/lease', { method: 'DELETE', keepalive: true,
    headers: { Authorization: `Bearer ${token}`, 'X-Editor-Window': windowId, 'X-Editor-Lease': current } }).catch(() => {});
}
function updateFeedback() {
  if (!draft) return;
  const sync = element('sync-status');
  sync.textContent = !connected ? t('Not connected · export available', '未连接 · 可导出草稿')
    : !canEdit() ? t('Read-only window', '只读窗口')
    : generation !== persistedGeneration ? t('Draft not synced', '草稿未同步')
    : draft.draftId ? t('Draft stored locally', '草稿已存到本机') : t('No changes · no draft', '无更改 · 未创建草稿');
  sync.classList.toggle('error', !connected || generation !== persistedGeneration);
  element('validation-label').textContent = ({ none: t('Validate', '校验'), running: t('Validating', '正在校验'),
    passed: t('Validated', '校验通过'), failed: t('Validation failed', '校验失败'), stale: t('Revalidate', '校验更改') })[validationState];
  element('validate').classList.toggle('error', validationState === 'failed');
  element('validate').title = validationState === 'passed' ? t('Validate current content again', '重新校验当前内容')
    : validationState === 'stale' ? t('Changes need validation', '最新更改待校验') : t('Validate current content', '校验当前内容');
}
function report(error: unknown) { status(error instanceof Error ? error.message : String(error), true); }
function command(id: string, action: () => void | Promise<void>) { element(id).onclick = () => { Promise.resolve().then(action).catch(report); }; }
function button(id: string, icon: string, title: string) { return `<button id="${id}" title="${title}" aria-label="${title}"><i data-lucide="${icon}"></i></button>`; }
function buildUi() {
  element('app').innerHTML = `<header><strong>MQTT Plus</strong><span>${t('Processor Editor', '脚本编辑器')}</span><div class="spacer"></div>${button('import', 'upload', t('Import draft', '导入草稿'))}${button('export', 'download', t('Export draft', '导出草稿'))}<button id="validate"><i data-lucide="check-check"></i><span id="validation-label" role="status">${t('Validate', '校验')}</span></button><button id="save" class="primary"><i data-lucide="save"></i>${t('Save version', '保存版本')}</button></header>
    <section id="connection-banner" role="alert" hidden><span id="connection-message"></span><button id="connection-export"><i data-lucide="download"></i>${t('Export draft', '导出草稿')}</button></section>
    <section class="metadata" id="metadata"><label>${t('Name', '名称')}<input id="name" maxlength="256"></label><label class="description">${t('Description', '描述')}<input id="description" maxlength="4096"></label><label>${t('Language', '语言')}<select id="language"><option value="lua">Lua 5.5</option><option value="javascript">JavaScript / Qt</option></select></label></section>
    <main><aside id="file-panel"><div class="file-toolbar"><strong>${t('Files', '文件')}</strong><div class="spacer"></div>${button('add', 'file-plus', t('Add file', '新增文件'))}${button('rename', 'pencil', t('Rename file', '重命名文件'))}${button('remove', 'trash-2', t('Delete file', '删除文件'))}</div><nav id="files" aria-label="${t('Source files', '源码文件')}"></nav><label class="entry-label">${t('Entry file', '入口文件')}<select id="entry"></select></label><label class="entry-label">${t('Entry symbol', '入口函数')}<input id="symbol" maxlength="128" aria-describedby="symbol-error"></label><span id="symbol-error" class="field-error" hidden></span><div class="spacer"></div><button id="recover"><i data-lucide="history"></i>${t('Drafts', '草稿')}</button><button id="discard"><i data-lucide="trash-2"></i>${t('Discard draft', '丢弃草稿')}</button></aside><section class="code-area"><div id="file-title"></div><div id="editor"></div><div id="diagnostics" aria-live="polite"></div></section></main><footer><span id="sync-status" role="status"></span><span id="status" role="status"></span><div class="spacer"></div><span id="version"></span></footer><dialog id="dialog" aria-labelledby="dialog-title"><h2 id="dialog-title"></h2><div id="dialog-body"></div><div id="dialog-actions"></div></dialog><input id="import-file" type="file" accept=".json" hidden>`;
  const cancel = document.createElement('button'); cancel.id = 'cancel'; cancel.hidden = true; cancel.title = t('Cancel validation', '取消校验'); cancel.setAttribute('aria-label', cancel.title); cancel.innerHTML = '<i data-lucide="x"></i>'; element('save').before(cancel);
  const switcher = document.createElement('button'); switcher.id = 'processors'; switcher.title = t('Switch processor', '切换脚本'); switcher.setAttribute('aria-label', switcher.title);
  switcher.setAttribute('aria-haspopup', 'dialog');
  switcher.innerHTML = `<i data-lucide="folder-open"></i><span id="processor-name">${t('Processors', '脚本')}</span><i data-lucide="chevron-down"></i>`;
  element('app').querySelector('header strong')!.after(switcher);
  element('file-title').innerHTML = `<span id="file-name"></span><div class="file-actions">${button('toggle-files', 'panel-left', t('Files and entry', '文件与入口'))}${button('toggle-metadata', 'settings-2', t('Processor details', '脚本信息'))}${button('toggle-wrap', 'wrap-text', t('Word wrap', '自动换行'))}</div>`;
  createIcons({ icons: { Upload, Download, CheckCheck, Save, FilePlus, Pencil, Trash2, History, X, FolderOpen, ChevronDown, PanelLeft, Settings2, WrapText } });
  editor = monaco.editor.create(element('editor'), { readOnly: true, automaticLayout: true, minimap: { enabled: false }, fontSize: 13, fontFamily: 'Menlo, Consolas, monospace', scrollBeyondLastLine: false, theme: (theme === 'dark' || (theme === 'system' && matchMedia('(prefers-color-scheme: dark)').matches)) ? 'vs-dark' : 'vs', wordWrap: 'off', model: null });
  editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => { submit('save').catch(report); });
  editor.onDidCompositionStart(() => composition('editor', true));
  editor.onDidCompositionEnd(() => composition('editor', false));
  command('save', () => submit('save')); command('validate', () => submit('validate'));
  command('processors', openProcessors);
  command('cancel', async () => { if (activeJob) await api(`/jobs/${activeJob}`, 'DELETE'); });
  command('export', exportDraft); command('import', () => { if (!pendingJob && !navigating && !importing) element<HTMLInputElement>('import-file').click(); });
  command('connection-export', exportDraft);
  const narrow = matchMedia('(max-width: 700px)');
  function panels() {
    for (const [id, panel, className] of [['toggle-files', 'file-panel', 'files-open'], ['toggle-metadata', 'metadata', 'metadata-open']]) {
      const open = !narrow.matches || element('app').classList.contains(className);
      element(panel).hidden = !open; element(id).setAttribute('aria-expanded', String(open)); element(id).setAttribute('aria-controls', panel);
    }
  }
  for (const [id, className] of [['toggle-files', 'files-open'], ['toggle-metadata', 'metadata-open']]) command(id, () => { element('app').classList.toggle(className); panels(); });
  narrow.addEventListener('change', panels); panels();
  element('toggle-wrap').setAttribute('aria-pressed', 'false');
  command('toggle-wrap', () => {
    const wrap = element('toggle-wrap').getAttribute('aria-pressed') !== 'true';
    element('toggle-wrap').setAttribute('aria-pressed', String(wrap)); editor.updateOptions({ wordWrap: wrap ? 'on' : 'off' });
  });
  command('add', addFile); command('rename', renameFile); command('remove', removeFile); command('recover', () => recovery(undefined, true));
  command('discard', discardDraft);
  for (const id of ['name', 'description', 'symbol']) {
    const input = element<HTMLInputElement>(id);
    input.addEventListener('compositionstart', () => composition(id, true));
    input.addEventListener('compositionend', () => composition(id, false));
    input.oninput = () => {
      if (!canEdit()) { input.value = id === 'symbol' ? draft.entrySymbol : draft[id as 'name' | 'description']; return; }
      if (id === 'symbol') draft.entrySymbol = input.value;
      else draft[id as 'name' | 'description'] = input.value;
      if (id === 'name') element('processor-name').textContent = input.value || t('New processor', '新脚本');
      changed();
    };
  }
  element<HTMLSelectElement>('entry').onchange = () => {
    if (!canEdit()) { element<HTMLSelectElement>('entry').value = draft.entryFile; return; }
    draft.entryFile = element<HTMLSelectElement>('entry').value; changed();
  };
  element<HTMLSelectElement>('language').onchange = async () => {
    const language = element<HTMLSelectElement>('language').value;
    element<HTMLSelectElement>('language').value = draft.languageId;
    if (!canEdit()) return;
    if (!confirm(t('Replace all files with the selected language template?', '将所有文件替换为所选语言的模板？'))) return;
    if (!canEdit()) return;
    const template = structuredClone(templates.find(item => item.languageId === language)!);
    draft = { ...draft, ...template, name: draft.name, description: draft.description };
    rebuildModels(); render(); changed();
  };
  element<HTMLInputElement>('import-file').onchange = () => { importDraft().catch(report); };
  element('editor').addEventListener('beforeinput', event => { if (!canEdit()) { event.preventDefault(); setLease('', 0); } }, true);
  updateControls();
}

function registerLua() {
  monaco.languages.registerCompletionItemProvider('lua', {
    provideCompletionItems(model, position) {
      const word = model.getWordUntilPosition(position);
      const range = { startLineNumber: position.lineNumber, endLineNumber: position.lineNumber, startColumn: word.startColumn, endColumn: word.endColumn };
      return { suggestions: ['context', 'context.decoded', 'context.topic', 'context.payload', 'context.parameters', 'string', 'table', 'pairs', 'ipairs', 'tonumber', 'tostring'].map(label => ({ label, insertText: label, kind: monaco.languages.CompletionItemKind.Variable, range })) };
    }
  });
  monaco.languages.typescript.javascriptDefaults.setCompilerOptions({ allowNonTsExtensions: true, checkJs: false, target: monaco.languages.typescript.ScriptTarget.ES2020 });
}
function rebuildModels() {
  editor.setModel(null);
  for (const model of models.values()) model.dispose();
  models.clear();
  for (const file of draft.files) {
    attachModel(file);
  }
  selected = models.has(selected) ? selected : draft.entryFile;
}
function attachModel(file: SourceFile) {
  const model = monaco.editor.createModel(file.content, draft.languageId, monaco.Uri.parse(`inmemory://editor/${crypto.randomUUID()}/${file.path}`));
  model.onDidChangeContent(changed);
  models.set(file.path, model);
}
function snapshotDraft(): Draft {
  const snapshot = structuredClone(draft);
  for (const file of snapshot.files) file.content = models.get(file.path)?.getValue() ?? file.content;
  return snapshot;
}
function render() {
  element('processor-name').textContent = draft.name || t('New processor', '新脚本');
  for (const id of ['name', 'description']) element<HTMLInputElement>(id).value = draft[id as 'name' | 'description'];
  element<HTMLInputElement>('symbol').value = draft.entrySymbol;
  element<HTMLSelectElement>('language').value = draft.languageId;
  element('files').replaceChildren(); element('entry').replaceChildren();
  for (const file of draft.files) {
    const item = document.createElement('button'); item.textContent = file.path; item.title = file.path; item.classList.toggle('selected', file.path === selected);
    item.setAttribute('aria-current', String(file.path === selected));
    item.onclick = () => { selected = file.path; render(); if (matchMedia('(max-width: 700px)').matches) { element('file-panel').hidden = true; element('app').classList.remove('files-open'); element('toggle-files').setAttribute('aria-expanded', 'false'); editor.focus(); } }; element('files').append(item);
    const option = document.createElement('option'); option.value = file.path; option.textContent = file.path; element('entry').append(option);
  }
  element<HTMLSelectElement>('entry').value = draft.entryFile;
  editor.setModel(models.get(selected) || null);
  element('file-name').textContent = selected;
  element('file-name').title = selected;
  element('version').textContent = draft.baseVersion ? t(`Base version ${draft.baseVersion}`, `基础版本 ${draft.baseVersion}`) : t('New processor', '新脚本');
  updateFeedback();
}
function composition(owner: string, active: boolean) {
  if (active) {
    compositions.begin(owner);
    clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined;
  } else {
    compositions.end(owner);
    if (!compositions.active && generation !== persistedGeneration) changed();
  }
  updateControls();
}
function updateControls() {
  applyEditingLock(canEdit(), editor, ['name', 'description', 'symbol'].map(id => element<HTMLInputElement>(id)),
    ['add', 'rename', 'remove', 'entry', 'import-file'].map(id => element<HTMLButtonElement | HTMLSelectElement | HTMLInputElement>(id)));
  for (const id of ['save', 'validate', 'processors', 'recover', 'discard', 'import', 'language']) {
    element<HTMLButtonElement | HTMLSelectElement>(id).disabled = !canEdit() || pendingJob || navigating || importing || compositions.active;
  }
  element<HTMLButtonElement>('cancel').disabled = !canEdit();
  element('dialog').querySelectorAll<HTMLButtonElement>('[data-mutation]').forEach(control => { control.disabled = dialogLoading || !canEdit(); });
  if (draft && !draft.draftId && generation === persistedGeneration) element<HTMLButtonElement>('discard').disabled = true;
}
function changed() {
  ++generation;
  if (!pendingJob) status('');
  if (validationState !== 'none' && validationState !== 'running') { validationState = 'stale'; showDiagnostics([]); }
  updateFeedback();
  updateControls();
  if (compositions.active || !canEdit()) return;
  clearTimeout(debounce); debounce = setTimeout(() => { flush().catch(report); }, 1000);
  if (!maxWait) maxWait = setTimeout(() => { maxWait = undefined; flush().catch(report); }, 5000);
}
function remember() {
  sessionStorage.setItem('draftId', draft.draftId);
  const processorId = draft.processorId || '';
  if (draft.draftId) draftHistory.set(processorId, draft.draftId);
  else draftHistory.delete(processorId);
  sessionStorage.setItem('draftHistory', JSON.stringify([...draftHistory]));
}
function flush(): Promise<void> {
  clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined;
  const run = async () => {
    if (compositions.active || finalizing || generation === persistedGeneration) return;
    if (!canEdit()) throw new ApiError(423, !connected ? connectionMessage : t('This window is read-only.', '本窗口只读。'));
    const snapshot = snapshotDraft(), current = generation;
    if (!needsDraftWrite(snapshot, acknowledged)) { persistedGeneration = current; updateFeedback(); updateControls(); return; }
    try {
      const saved = draft.draftId ? await api<Draft>(`/drafts/${draft.draftId}`, 'PUT', snapshot, draft.draftVersion)
        : await api<Draft>('/drafts', 'POST', snapshot);
      draft.draftId = saved.draftId; draft.draftVersion = saved.draftVersion; draft.updatedAt = saved.updatedAt; remember();
      acknowledged = saved;
    } catch (error) {
      if (!(error instanceof ApiError) || (error.status !== 409 && error.status !== 404)) throw error;
      const branch = await api<Draft>('/drafts', 'POST', snapshot);
      draft.draftId = branch.draftId; draft.draftVersion = branch.draftVersion; remember();
      acknowledged = branch;
    }
    persistedGeneration = current;
    updateFeedback(); updateControls();
  };
  const next = writing.then(run, run); writing = next.catch(() => {}); return next;
}
function showDiagnostics(items: Diagnostic[]) {
  element('diagnostics').replaceChildren();
  element('symbol-error').hidden = true; element('symbol-error').textContent = ''; element('symbol').removeAttribute('aria-invalid');
  for (const model of models.values()) monaco.editor.setModelMarkers(model, 'runtime', []);
  for (const item of items) {
    const missingEntry = isMissingEntry(item, draft.entrySymbol);
    const summary = missingEntry ? t(`Entry function ${draft.entrySymbol}(context) is missing`, `未找到入口函数 ${draft.entrySymbol}(context)`)
      : t('Runtime validation failed', '运行时校验失败');
    if (missingEntry) { element('symbol-error').textContent = summary; element('symbol-error').hidden = false; element('symbol').setAttribute('aria-invalid', 'true'); }
    const row = document.createElement('button');
    const title = document.createElement('strong'); title.textContent = `${item.file || ''}${item.line && item.line > 0 ? ':' + item.line : ''} · ${summary}`;
    const details = document.createElement('span'); details.textContent = item.message; row.append(title, details);
    row.onclick = () => {
      if (missingEntry) { element('app').classList.add('files-open'); element('file-panel').hidden = false; element('toggle-files').setAttribute('aria-expanded', 'true'); element('symbol').focus(); }
      else if (item.file && models.has(item.file)) { selected = item.file; render(); editor.revealLineInCenter(Math.max(1, item.line || 1)); editor.setPosition({ lineNumber: Math.max(1, item.line || 1), column: Math.max(1, item.column || 1) }); editor.focus(); }
    };
    element('diagnostics').append(row);
    const model = item.file ? models.get(item.file) : undefined;
    if (model && item.line && item.line > 0) monaco.editor.setModelMarkers(model, 'runtime', [...monaco.editor.getModelMarkers({ resource: model.uri, owner: 'runtime' }), {
      severity: monaco.MarkerSeverity.Error, message: item.message, startLineNumber: item.line, endLineNumber: item.line,
      startColumn: Math.max(1, item.column || 1), endColumn: Math.max(2, (item.column || 1) + 1) }]);
  }
}
async function submit(operation: 'validate' | 'save') {
  if (!canEdit() || pendingJob || navigating || importing || compositions.active) return;
  pendingJob = true; updateControls();
  let validationOnlyDraft: Draft | undefined;
  try {
    await flush();
    let frozenGeneration = persistedGeneration;
    let jobId = '';
    let job: Job | undefined;
    const pending = operation === 'save' ? JSON.parse(sessionStorage.getItem('pendingSave') || 'null') : null;
    if (pending) {
      frozenGeneration = pending.generation;
      try {
        const result = await api<NonNullable<Job['result']>>(`/requests/${encodeURIComponent(pending.requestId)}`);
        job = { state: 'completed', status: 200, result };
      } catch (error) {
        if (!(error instanceof ApiError) || error.status !== 404) throw error;
        try { jobId = (await api<{ jobId: string }>(`/jobs?requestId=${encodeURIComponent(pending.requestId)}`)).jobId; }
        catch (lookupError) {
          if (!(lookupError instanceof ApiError) || lookupError.status !== 404) throw lookupError;
          sessionStorage.removeItem('pendingSave');
        }
      }
    }
    if (!jobId && !job) {
      if (!draft.draftId) {
        if (operation === 'save' && draft.processorId && generation === persistedGeneration) { status(t('No changes to save', '没有需要保存的更改')); return; }
        const stored = await api<Draft>('/drafts', 'POST', snapshotDraft());
        if (operation === 'validate') validationOnlyDraft = structuredClone(stored);
        draft.draftId = stored.draftId; draft.draftVersion = stored.draftVersion; acknowledged = stored; remember();
      }
      const requestId = crypto.randomUUID();
      frozenGeneration = persistedGeneration;
      if (operation === 'save') sessionStorage.setItem('pendingSave', JSON.stringify({ requestId, generation: frozenGeneration }));
      jobId = (await api<{ jobId: string }>(`/drafts/${draft.draftId}/${operation}`, 'POST', { draftVersion: draft.draftVersion, requestId })).jobId;
    }
    validationGeneration = frozenGeneration; validationState = 'running'; updateFeedback(); status(t('Validating', '正在校验'));
    activeJob = jobId; element('cancel').hidden = !jobId;
    while (!job || (job.state !== 'completed' && job.state !== 'cancelled')) { await new Promise(resolve => setTimeout(resolve, 300)); job = await api<Job>(`/jobs/${jobId}`); }
    const currentValidation = validationGeneration === generation;
    showDiagnostics(currentValidation ? job.validation?.diagnostics || [] : []);
    validationState = currentValidation ? (job.status === 200 ? 'passed' : 'failed') : 'stale'; updateFeedback();
    if (job.state === 'cancelled') { validationState = 'none'; updateFeedback(); sessionStorage.removeItem('pendingSave'); status(t('Validation cancelled; draft retained', '校验已取消，草稿已保留')); return; }
    if (job.status !== 200) {
      if (job.status === 423) setLease('', 0);
      sessionStorage.removeItem('pendingSave');
      if (job.status === 409) await conflict();
      throw new Error(job.result?.error || t('Validation failed. Draft is retained.', '校验失败，草稿已保留。'));
    }
    if (operation === 'save' && job.result?.processor) {
      const saved = job.result.processor;
      // Stop queuing writes while reconciling the committed snapshot with newer input.
      finalizing = true;
      await writing;
      sessionStorage.setItem('target', JSON.stringify({ processorId: saved.id, languageId: draft.languageId }));
      let surviving: Draft | undefined;
      if (draft.draftId) { try { surviving = await api<Draft>(`/drafts/${draft.draftId}`); } catch (error) { if (!(error instanceof ApiError) || error.status !== 404) throw error; } }
      const decision = afterSave(snapshotDraft(), acknowledged, surviving, saved);
      draft.processorId = saved.id; draft.baseVersion = saved.version;
      if (decision === 'reuse') { draft.draftVersion = surviving!.draftVersion; }
      else { draft.draftId = ''; draft.draftVersion = ''; }
      finalizing = false;
      remember(); sessionStorage.removeItem('pendingSave'); render();
      if (decision !== 'clear') { persistedGeneration = -1; await flush(); }
      else { persistedGeneration = generation; acknowledged = snapshotDraft(); }
      status(decision === 'clear' ? t('Version saved', '版本已保存')
        : generation !== persistedGeneration ? t('Version saved; newer edits are unsynced', '版本已保存，后续更改未同步')
        : t('Version saved; newer edits remain in draft', '版本已保存，后续更改保留在草稿中'));
      if (job.result.warnings?.length) status(`${t('Version saved', '版本已保存')}: ${job.result.warnings.join(' ')}`, true);
    } else status(currentValidation ? t('Validation passed', '校验通过') : t('Previous content validated; newer changes need validation', '先前内容校验通过，后续更改待校验'));
  } finally {
    if (validationOnlyDraft && canEdit()) {
      // Validation needs a stored snapshot, but viewing/validating unchanged content must not leave a branch.
      await writing;
      if (draft.draftId === validationOnlyDraft.draftId && draft.draftVersion === validationOnlyDraft.draftVersion
        && sameContent(snapshotDraft(), validationOnlyDraft)) {
        try {
          await api(`/drafts/${validationOnlyDraft.draftId}`, 'DELETE', undefined, validationOnlyDraft.draftVersion);
          draft.draftId = ''; draft.draftVersion = ''; persistedGeneration = generation; acknowledged = snapshotDraft(); remember();
        } catch (error) { report(error); }
      }
    }
    if (validationState === 'running') validationState = 'none'; finalizing = false; activeJob = ''; element('cancel').hidden = true; pendingJob = false; updateControls(); updateFeedback();
  }
}
async function conflict() {
  if (!draft.processorId) return;
  const current = await api<{ package: Package }>(`/processors/${draft.processorId}`);
  const dialog = element<HTMLDialogElement>('dialog'); element('dialog-title').textContent = t('Processor changed', '脚本已被修改');
  element('dialog-body').replaceChildren(); element('dialog-actions').replaceChildren();
  const area = document.createElement('div'); area.style.height = '360px'; element('dialog-body').append(area);
  const diff = monaco.editor.createDiffEditor(area, { automaticLayout: true, readOnly: true, minimap: { enabled: false } });
  const oldModel = monaco.editor.createModel(current.package.files.find(file => file.path === selected)?.content || '', draft.languageId);
  const newModel = monaco.editor.createModel(models.get(selected)?.getValue() || '', draft.languageId);
  diff.setModel({ original: oldModel, modified: newModel });
  const close = document.createElement('button'); close.textContent = t('Keep draft', '保留草稿'); close.onclick = () => dialog.close();
  const copy = document.createElement('button'); copy.textContent = t('Save as new processor', '另存为新脚本');
  copy.dataset.mutation = 'true';
  copy.onclick = () => {
    copy.disabled = true;
    flush().then(() => api<Draft>('/drafts', 'POST', { ...snapshotDraft(), processorId: '', baseVersion: '' }))
      .then(value => { load(value); dialog.close(); }).catch(report).finally(() => { copy.disabled = false; });
  };
  element('dialog-actions').append(close, copy); dialog.onclose = () => { diff.dispose(); oldModel.dispose(); newModel.dispose(); }; dialog.showModal();
}
function load(value: Draft) {
  clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined;
  acknowledged = structuredClone(value);
  sessionStorage.setItem('target', JSON.stringify({ processorId: value.processorId || '', languageId: value.languageId }));
  draft = value; generation = persistedGeneration = 0; validationState = 'none'; remember(); rebuildModels(); render(); showDiagnostics([]); status(''); updateFeedback(); updateControls();
}
async function recovery(target?: { processorId?: string; languageId?: string }, chooseBranch = false) {
  if (compositions.active) return;
  if (!target && (pendingJob || importing)) return;
  if (!target) await flush();
  const id = target?.processorId ?? draft?.processorId ?? '';
  const result = await api<{ drafts: DraftSummary[] }>(`/drafts?processorId=${encodeURIComponent(id)}`);
  const create = async (persist = false) => {
    const languageId = target?.languageId ?? draft?.languageId ?? 'lua';
    const template = structuredClone(templates.find(item => item.languageId === languageId)!);
    template.name = t(`New ${languageId === 'lua' ? 'Lua' : 'JavaScript'} Processor`, `新建 ${languageId === 'lua' ? 'Lua' : 'JavaScript'} 脚本`);
    load(await openDraftContent(api, { processorId: id, languageId }, template, persist));
  };
  if (!result.drafts.length && !chooseBranch) { await create(); if (navigating) element<HTMLDialogElement>('dialog').close(); return; }
  const preferredId = chooseBranch ? undefined : chooseRecoveryDraft(result.drafts, draftHistory.get(id));
  if (preferredId) {
    load(await api<Draft>(`/drafts/${preferredId}`));
    if (navigating) element<HTMLDialogElement>('dialog').close();
    return;
  }
  const dialog = element<HTMLDialogElement>('dialog'); element('dialog-title').textContent = t('Restore draft', '恢复草稿');
  dialog.oncancel = event => { if (!draft || dialogLoading) event.preventDefault(); };
  element('dialog-body').replaceChildren(); element('dialog-actions').replaceChildren(); dialog.onclose = null;
  if (!result.drafts.length) { const empty = document.createElement('p'); empty.textContent = t('No drafts', '暂无草稿'); element('dialog-body').append(empty); }
  for (const item of result.drafts) {
    const row = document.createElement('div'); row.className = 'draft-row';
    const restore = document.createElement('button'); restore.className = 'draft-restore';
    const name = document.createElement('strong'); name.textContent = `${item.name}${item.draftId === draft?.draftId ? t(' · Current branch', ' · 当前分支') : ''}`;
    const summary = document.createElement('span'); summary.textContent = [formatDraftTime(item.updatedAt, zh ? 'zh-CN' : 'en'),
      t(`${item.fileCount} files`, `${item.fileCount} 个文件`), item.baseVersion ? t(`Base version ${item.baseVersion}`, `基础版本 ${item.baseVersion}`) : t('New processor', '新脚本'),
      t(`Branch ${item.draftId.slice(0, 8)}`, `分支 ${item.draftId.slice(0, 8)}`)].filter(Boolean).join(' · ');
    const detail = document.createElement('span'); detail.textContent = item.description || `${item.entryFile} · ${item.entrySymbol}(context)`;
    restore.append(name, summary, detail);
    restore.onclick = () => { setDialogLoading(true); api<Draft>(`/drafts/${item.draftId}`).then(value => { load(value); dialog.close(); }).catch(report).finally(() => setDialogLoading(false)); };
    const preview = document.createElement('button'); preview.textContent = t('Preview', '预览');
    preview.onclick = () => { previewDraft(item, id).catch(report); };
    const remove = document.createElement('button'); remove.textContent = t('Discard', '丢弃'); remove.className = 'quiet-danger'; remove.dataset.mutation = 'true';
    remove.onclick = () => {
      if (!confirm(t(`Permanently discard branch ${item.draftId.slice(0, 8)} of ${item.name}?`, `永久丢弃「${item.name}」的分支 ${item.draftId.slice(0, 8)}？`))) return;
      setDialogLoading(true); api(`/drafts/${item.draftId}`, 'DELETE', undefined, item.draftVersion).then(async () => {
        if (item.draftId === draft?.draftId) await create();
        row.remove();
      }).catch(report).finally(() => setDialogLoading(false));
    };
    row.append(restore, preview, remove); element('dialog-body').append(row);
  }
  const fresh = document.createElement('button'); fresh.dataset.mutation = 'true'; fresh.textContent = t('New branch', '新建分支'); fresh.onclick = () => { setDialogLoading(true); create(true).then(() => dialog.close()).catch(report).finally(() => setDialogLoading(false)); }; element('dialog-actions').append(fresh);
  if (draft) { const cancel = document.createElement('button'); cancel.textContent = t('Cancel', '取消'); cancel.onclick = () => dialog.close(); element('dialog-actions').prepend(cancel); }
  if (!dialog.open) dialog.showModal();
  updateControls();
  if (!dialogLoading) element('dialog-body').querySelector<HTMLButtonElement>('button')?.focus();
}

async function previewDraft(item: DraftSummary, processorId: string) {
  setDialogLoading(true);
  try {
    const value = await api<Draft>(`/drafts/${item.draftId}`);
    element('dialog-title').textContent = t('Preview draft', '预览草稿');
    const body = element('dialog-body'); body.replaceChildren();
    for (const file of value.files) {
      const heading = document.createElement('h3'); heading.textContent = file.path;
      const source = document.createElement('pre'); source.className = 'draft-source'; source.textContent = file.content; body.append(heading, source);
    }
    const back = document.createElement('button'); back.textContent = t('Back', '返回'); back.onclick = () => { recovery({ processorId, languageId: value.languageId }, true).catch(report); };
    const restore = document.createElement('button'); restore.className = 'primary'; restore.textContent = t('Restore draft', '恢复草稿');
    restore.onclick = () => { setDialogLoading(true); api<Draft>(`/drafts/${item.draftId}`).then(latest => { load(latest); element<HTMLDialogElement>('dialog').close(); }).catch(report).finally(() => setDialogLoading(false)); };
    element('dialog-actions').replaceChildren(back, restore);
    setDialogLoading(false); back.focus();
  } finally { setDialogLoading(false); }
}

function setDialogLoading(value: boolean) {
  dialogLoading = value;
  element('dialog').querySelectorAll<HTMLButtonElement | HTMLInputElement>('button,input').forEach(control => { control.disabled = value; });
  updateControls();
  if (!value && element<HTMLDialogElement>('dialog').open && !element('dialog').contains(document.activeElement)) {
    element('dialog').querySelector<HTMLButtonElement | HTMLInputElement>('input,button')?.focus();
  }
}
async function navigateToProcessor(target: { processorId: string; languageId: string }) {
  setDialogLoading(true);
  try { await recovery(target); } finally { setDialogLoading(false); }
}
async function openProcessors() {
  if (!canEdit() || pendingJob || navigating || importing || compositions.active) return;
  if (sessionStorage.getItem('pendingSave')) throw new Error(t('Retry the previous save before switching processors.', '请先重试上一次保存，再切换脚本。'));
  navigating = true; updateControls();
  const dialog = element<HTMLDialogElement>('dialog');
  element('dialog-title').textContent = t('Switch processor', '切换脚本');
  element('dialog-body').textContent = t('Loading processors', '正在加载脚本'); element('dialog-actions').replaceChildren();
  const cancel = document.createElement('button'); cancel.textContent = t('Cancel', '取消'); cancel.onclick = () => dialog.close(); element('dialog-actions').append(cancel);
  const unsaved = document.createElement('button'); unsaved.textContent = t('Unsaved processor drafts', '未保存脚本草稿');
  unsaved.onclick = () => { navigateToProcessor({ processorId: '', languageId: draft?.languageId || 'lua' }).catch(report); }; element('dialog-actions').prepend(unsaved);
  dialog.oncancel = event => { if (dialogLoading) event.preventDefault(); };
  dialog.onclose = null;
  dialog.addEventListener('close', () => { navigating = false; updateControls(); }, { once: true });
  setDialogLoading(true); dialog.showModal();
  try {
    await flush();
    const result = await api<{ processors: ProcessorSummary[] }>('/processors');
    const search = document.createElement('input'); search.type = 'search'; search.placeholder = t('Search processors', '搜索脚本'); search.setAttribute('aria-label', search.placeholder);
    const list = document.createElement('div'); list.className = 'processor-list';
    const renderList = () => {
      list.replaceChildren();
      for (const item of filterProcessors(result.processors, search.value)) {
        const row = document.createElement('button'); row.className = 'processor-row'; row.setAttribute('aria-current', String(item.id === draft?.processorId));
        const name = document.createElement('span'); const title = document.createElement('strong'); title.textContent = item.name;
        const description = document.createElement('small'); description.textContent = item.description; name.append(title, description);
        const language = document.createElement('small'); language.textContent = `${item.languageId === 'javascript' ? 'JavaScript' : 'Lua'} · ${t('Version', '版本')} ${item.version}`; row.append(name, language);
        row.onclick = () => {
          if (item.id === draft?.processorId) { dialog.close(); return; }
          navigateToProcessor({ processorId: item.id, languageId: item.languageId }).catch(report);
        };
        list.append(row);
      }
      if (!list.childElementCount) { const empty = document.createElement('p'); empty.textContent = result.processors.length ? t('No matching processors', '没有匹配的脚本') : t('No saved processors', '暂无已保存的脚本'); list.append(empty); }
    };
    search.oninput = renderList; element('dialog-body').replaceChildren(search, list); renderList(); setDialogLoading(false); search.focus();
  } catch (error) {
    element('dialog-body').textContent = error instanceof Error ? error.message : String(error);
    setDialogLoading(false);
  }
}
async function discardDraft() {
  if (!canEdit() || pendingJob || navigating || importing || compositions.active || !confirm(t('Discard this draft?', '丢弃当前草稿？'))) return;
  clearTimeout(debounce); clearTimeout(maxWait); maxWait = undefined;
  await writing; if (draft.draftId) await api(`/drafts/${draft.draftId}`, 'DELETE', undefined, draft.draftVersion); await recovery({ processorId: draft.processorId, languageId: draft.languageId });
}
function exportDraft() {
  const data = { format: 'mqtt-plus.processor-draft/v1', package: Object.fromEntries(Object.entries(snapshotDraft()).filter(([key]) => !['draftId', 'draftVersion', 'processorId', 'baseVersion', 'updatedAt'].includes(key))) };
  const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' }));
  const anchor = document.createElement('a'); anchor.href = url; anchor.download = 'processor-draft.json'; anchor.click(); URL.revokeObjectURL(url);
}
async function importDraft() {
  if (!canEdit() || pendingJob || navigating || importing) return;
  const file = element<HTMLInputElement>('import-file').files?.[0]; if (!file) return;
  importing = true; updateControls();
  try {
    element<HTMLInputElement>('import-file').value = '';
    if (file.size > 16 * 1024 * 1024) throw new Error(t('Import is too large.', '导入文件过大。'));
    const data = JSON.parse(await file.text());
    if (data.format !== 'mqtt-plus.processor-draft/v1' || !data.package || !Array.isArray(data.package.files)) throw new Error(t('Unsupported draft format.', '不支持的草稿格式。'));
    const imported = await api<Draft>('/drafts', 'POST', { ...data.package, processorId: '', baseVersion: '' });
    do {
      if (compositions.active) await new Promise(resolve => setTimeout(resolve, 50));
      else await flush();
    } while (generation !== persistedGeneration);
    load(imported);
  } finally { importing = false; updateControls(); }
}
function pathInput(title: string, value = '') { return prompt(title, value)?.trim(); }
function validPath(path: string) { return path.length > 0 && !path.startsWith('/') && !path.includes('\\') && !path.split('/').some(part => !part || part === '..' || part === '.') && !draft.files.some(file => file.path.toLowerCase() === path.toLowerCase()); }
function addFile() {
  if (!canEdit()) return;
  const path = pathInput(t('File path', '文件路径')); if (!path) return;
  if (!canEdit()) return;
  if (!validPath(path)) throw new Error(t('Invalid or duplicate path.', '路径不合法或重复。'));
  const file = { path, mediaType: draft.languageId === 'lua' ? 'text/x-lua' : 'text/javascript', content: '' };
  draft.files.push(file); attachModel(file); selected = path; render(); changed();
}
function renameFile() {
  if (!canEdit()) return;
  const path = pathInput(t('New file path', '新文件路径'), selected); if (!path || path === selected) return;
  if (!canEdit()) return;
  if (!validPath(path)) throw new Error(t('Invalid or duplicate path.', '路径不合法或重复。'));
  const file = draft.files.find(item => item.path === selected)!; if (draft.entryFile === selected) draft.entryFile = path;
  const model = models.get(selected)!; models.delete(selected); models.set(path, model);
  file.path = path; selected = path; render(); changed();
}
function removeFile() {
  if (!canEdit()) return;
  if (draft.files.length <= 1 || !confirm(t(`Delete ${selected}?`, `删除 ${selected}？`))) return;
  if (!canEdit()) return;
  editor.setModel(null); models.get(selected)?.dispose(); models.delete(selected);
  draft.files = draft.files.filter(file => file.path !== selected); if (draft.entryFile === selected) draft.entryFile = draft.files[0].path;
  selected = draft.entryFile; render(); changed();
}
window.addEventListener('beforeunload', event => { if (generation !== persistedGeneration) { event.preventDefault(); event.returnValue = ''; } });
window.addEventListener('pagehide', releaseLease);
window.addEventListener('pageshow', event => { if (event.persisted) { pageLeaving = false; refreshLease().catch(report); } });
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible' && editor) {
    if (performance.now() >= leaseDeadline) setLease('', 0);
    refreshLease().catch(report);
  }
});
async function boot() {
  const fragment = new URLSearchParams(location.hash.slice(1)); const ticket = fragment.get('ticket');
  history.replaceState(null, '', location.pathname);
  let target = JSON.parse(sessionStorage.getItem('target') || '{}');
  if (ticket) {
    const session = await api<{ token: string; target: object }>('/sessions/exchange', 'POST', { ticket });
    token = session.token; target = session.target;
    sessionStorage.setItem('token', token); sessionStorage.setItem('target', JSON.stringify(target)); sessionStorage.removeItem('draftId');
    draftHistory.clear(); sessionStorage.removeItem('draftHistory');
  }
  const bootstrap = await api<{ language: string; theme: string; font: string; templates: Package[] }>('/bootstrap'); zh = bootstrap.language.startsWith('zh'); templates = bootstrap.templates; theme = bootstrap.theme;
  document.documentElement.dataset.theme = theme;
  document.documentElement.style.fontFamily = bootstrap.font;
  buildUi(); registerLua();
  await refreshLease();
  const previous = sessionStorage.getItem('draftId');
  setInterval(() => { if (document.visibilityState === 'visible') refreshLease().catch(report); }, 5000);
  if (previous) { try { load(await api<Draft>(`/drafts/${previous}`)); return; } catch (error) { if (!(error instanceof ApiError) || error.status !== 404) throw error; } }
  await recovery(target);
}
boot().catch(error => {
  if (!document.getElementById('status')) { const message = document.createElement('p'); message.className = 'boot-error'; message.textContent = error instanceof Error ? error.message : String(error); element('app').replaceChildren(message); }
  else report(error);
});
