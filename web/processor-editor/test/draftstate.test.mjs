import { test } from 'node:test';
import assert from 'node:assert/strict';
import { afterSave, chooseRecoveryDraft, sameContent, CompositionState, filterProcessors, formatDraftTime, isMissingEntry, openDraftContent, needsDraftWrite, applyEditingLock, hasEditingPermission } from '../.test-build/draftstate.js';

const draft = () => ({ draftId: 'demo', draftVersion: '2', processorId: 'processor', baseVersion: '1',
  name: 'Demo', description: '', languageId: 'lua', runtimeId: 'lua', entryFile: 'main.lua', entrySymbol: 'process',
  contractId: 'demo', manifest: {}, files: [{ path: 'main.lua', mediaType: 'text/x-lua', content: 'return {}' }] });
const saved = value => ({ id: 'processor', version: '2', package: structuredClone(value) });

test('editing requires a live lease, not just connectivity or a cached session', () => {
  assert.equal(hasEditingPermission(true, 'lease', 1000, 999, false), true);
  assert.equal(hasEditingPermission(true, 'lease', 1000, 1000, false), false);
  assert.equal(hasEditingPermission(true, 'lease', 1000, 1001, false), false);
  assert.equal(hasEditingPermission(true, '', 1000, 999, false), false);
  assert.equal(hasEditingPermission(false, 'lease', 1000, 999, false), false);
  assert.equal(hasEditingPermission(true, 'lease', 1000, 999, true), false);
});

test('disconnect locks source, metadata and file mutations without changing content', () => {
  const options = [];
  const editor = { updateOptions: value => options.push(value) };
  const fields = [{ readOnly: false, value: 'Demo' }, { readOnly: false, value: 'process' }];
  const controls = [{ disabled: false }, { disabled: false }];
  applyEditingLock(false, editor, fields, controls);
  assert.deepEqual(options, [{ readOnly: true }]);
  assert.equal(fields.every(field => field.readOnly), true);
  assert.equal(controls.every(control => control.disabled), true);
  assert.deepEqual(fields.map(field => field.value), ['Demo', 'process']);
  applyEditingLock(true, editor, fields, controls);
  assert.deepEqual(options.at(-1), { readOnly: false });
  assert.equal(fields.some(field => field.readOnly), false);
  assert.equal(controls.some(control => control.disabled), false);
});

test('opening an untouched saved processor reads its current version without creating a draft', async () => {
  const calls = [];
  const processor = saved(draft());
  const api = async (path, method = 'GET', body) => { calls.push({ path, method, body }); return method === 'GET' ? processor : draft(); };
  const content = await openDraftContent(api, { processorId: 'processor', languageId: 'lua' }, draft());
  assert.deepEqual(calls.map(({ path, method }) => [path, method]), [['/processors/processor', 'GET']]);
  assert.equal(content.draftId, '');
  assert.equal(content.baseVersion, '2');
  assert.equal(content.processorId, 'processor');
});

test('opening an untouched new template makes no request and preserves the template', async () => {
  const calls = [];
  const template = draft();
  const content = await openDraftContent(async (...args) => { calls.push(args); return draft(); }, {}, template);
  assert.deepEqual(calls, []);
  assert.equal(content.draftId, '');
  assert.equal(content.processorId, '');
  assert.equal(content.baseVersion, '');
  content.files[0].content = 'local edit';
  assert.equal(template.files[0].content, 'return {}');
});

test('an explicit new branch still persists even when its contents are unchanged', async () => {
  const calls = [];
  const result = await openDraftContent(async (...args) => { calls.push(args); return draft(); }, { processorId: 'processor', languageId: 'lua' }, draft(), true);
  assert.deepEqual(calls, [['/drafts', 'POST', { processorId: 'processor', languageId: 'lua' }]]);
  assert.equal(result.draftId, 'demo');
});

test('only real changes persist an unbranched baseline, including undo before autosave', async () => {
  const baseline = await openDraftContent(async () => saved(draft()), { processorId: 'processor' }, draft());
  const local = structuredClone(baseline);
  assert.equal(needsDraftWrite(local, baseline), false);
  local.description = 'Changed metadata';
  assert.equal(needsDraftWrite(local, baseline), true);
  local.description = baseline.description;
  assert.equal(needsDraftWrite(local, baseline), false);
  local.files[0].content += ' ';
  assert.equal(needsDraftWrite(local, baseline), true);
  local.files[0].content = baseline.files[0].content;
  assert.equal(needsDraftWrite(local, baseline), false);
  local.draftId = 'explicit-branch';
  assert.equal(needsDraftWrite(local, baseline), true);
});

test('discarding the last draft reloads the formal version without recreating a branch', async () => {
  const writes = [];
  const api = async (path, method = 'GET') => { if (method !== 'GET') writes.push(path); return saved(draft()); };
  const reset = await openDraftContent(api, { processorId: 'processor' }, draft());
  assert.equal(reset.draftId, '');
  assert.equal(needsDraftWrite(reset, structuredClone(reset)), false);
  assert.deepEqual(writes, []);
});

test('formats draft timestamps for readers rather than exposing raw UTC values', () => {
  const time = '2026-10-13T10:11:24.226Z';
  assert.equal(formatDraftTime(time, 'zh-CN'), new Intl.DateTimeFormat('zh-CN', { dateStyle: 'medium', timeStyle: 'short' }).format(new Date(time)));
  assert.equal(formatDraftTime(undefined, 'en'), '');
  assert.equal(formatDraftTime('invalid', 'en'), '');
});

test('associates only missing-entry diagnostics with the entry symbol', () => {
  for (const language of ['Lua', 'JavaScript']) {
    const item = { code: 'invalid_source', message: `${language} processor must define function process(context).` };
    assert.equal(isMissingEntry(item, 'process'), true);
    assert.equal(isMissingEntry(item, 'other'), false);
  }
  assert.equal(isMissingEntry({ code: 'invalid_source', message: 'Syntax error' }, 'process'), false);
});

test('normalizes metadata but never source whitespace', () => {
  const a = draft(), b = draft(); b.name = ' Demo '; b.manifest = { a: 1, b: 2 }; a.manifest = { b: 2, a: 1 };
  assert.equal(sameContent(a, b), true);
  b.files[0].content += ' ';
  assert.equal(sameContent(a, b), false);
});
test('saved content clears identity even after a reload', () => {
  const a = draft(); assert.equal(afterSave(a, a, undefined, saved(a)), 'clear');
});
test('unpersisted input after validation branches when submitted draft was removed', () => {
  const a = draft(), local = draft(); local.description = 'Later input';
  assert.equal(afterSave(local, a, undefined, saved(a)), 'branch');
});
test('only reuses a rebased draft whose content matches our acknowledged write', () => {
  const a = draft(), local = draft(); local.description = 'Later input';
  const remote = { ...structuredClone(local), baseVersion: '2', draftVersion: '4' };
  assert.equal(afterSave(local, local, remote, saved(a)), 'reuse');
  remote.description = 'Another tab';
  assert.equal(afterSave(local, local, remote, saved(a)), 'branch');
  remote.description = local.description; remote.baseVersion = '3';
  assert.equal(afterSave(local, local, remote, saved(a)), 'branch');
});
test('cannot reuse a different branch returned by a stale receipt', () => {
  const a = draft(), local = draft(); local.description = 'Later input';
  assert.equal(afterSave(local, local, { ...local, draftId: 'other', baseVersion: '2' }, saved(a)), 'branch');
});

test('composition blocks persistence until all input owners finish', () => {
  const state = new CompositionState();
  assert.equal(state.active, false);
  state.begin('name'); state.begin('name'); state.begin('editor');
  assert.equal(state.active, true);
  state.end('name');
  assert.equal(state.active, true);
  state.end('editor'); state.end('editor');
  assert.equal(state.active, false);
});

test('processor picker searches metadata and languages without mutating its snapshot', () => {
  const rows = [
    { id: '2', name: 'Telemetry', description: 'Device frames', languageId: 'javascript', version: '1' },
    { id: '1', name: '中文脚本', description: '温度', languageId: 'lua', version: '2' }
  ];
  const before = structuredClone(rows);
  assert.deepEqual(filterProcessors(rows, ' JAVASCRIPT ').map(row => row.id), ['2']);
  assert.deepEqual(filterProcessors(rows, '温度').map(row => row.id), ['1']);
  assert.deepEqual(filterProcessors(rows, 'device').map(row => row.id), ['2']);
  assert.deepEqual(filterProcessors(rows, 'unknown'), []);
  assert.equal(filterProcessors(rows, '').length, 2);
  assert.deepEqual(rows, before);
});

test('recovery automatically selects the only available draft', () => {
  assert.equal(chooseRecoveryDraft([]), undefined);
  assert.equal(chooseRecoveryDraft([{ draftId: 'only' }]), 'only');
  assert.equal(chooseRecoveryDraft([{ draftId: 'only' }], 'deleted'), 'only');
});

test('recovery prefers this tab branch but never guesses between unfamiliar branches', () => {
  const branches = [{ draftId: 'first' }, { draftId: 'second' }];
  assert.equal(chooseRecoveryDraft(branches, 'second'), 'second');
  assert.equal(chooseRecoveryDraft(branches), undefined);
  assert.equal(chooseRecoveryDraft(branches, 'deleted'), undefined);
});
