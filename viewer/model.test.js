'use strict';
// Run with: node --test viewer/model.test.js
const test = require('node:test');
const assert = require('node:assert');

global.window = {};
require('./sample.js');
const { buildModel, parseLines, groupRegions, sizeText } = require('./model.js');

const sample = global.window.SYSSIGHT_SAMPLE;

const region = (start, end, kind, name, permissions = 'rw-p') => ({ start, end, kind, name, permissions });

function stream(snapshot, extra = []) {
  return [{ schema_version: 1 }, ...extra, { snapshot }].map((line) => JSON.stringify(line)).join('\n');
}

test('sample: frames are placed in the stack row, innermost first', () => {
  const model = buildModel(sample);
  assert.ok(model.ok);
  assert.deepStrictEqual(model.frames.map((frame) => frame.function), ['inner', 'outer', 'main']);
  const stackRow = model.rows.find((row) => row.kind === 'stack');
  assert.strictEqual(stackRow.frames.length, 3);
  for (let index = 1; index < model.frames.length; index++) {
    assert.strictEqual(model.frames[index].startBig, model.frames[index - 1].endBig);
  }
  assert.strictEqual(model.stackEnd, 'left_executable');
});

test('sample: globals are in the table with their regions and values', () => {
  const model = buildModel(sample);
  const byName = Object.fromEntries(model.table.map((row) => [row.name, row]));
  assert.strictEqual(byName.g_data.region, 'data');
  assert.strictEqual(byName.g_data.section, '.data');
  assert.strictEqual(byName.g_data.decimal, '7');
  assert.strictEqual(byName.g_bss.section, '.bss');
  assert.strictEqual(byName.g_bss.decimal, '0');
});

test('toolchain symbols are hidden unless requested', () => {
  const hidden = buildModel(sample);
  assert.ok(hidden.hiddenSymbols > 0);
  assert.ok(!hidden.table.some((row) => row.name.startsWith('_')));
  const shown = buildModel(sample, { showToolchain: true });
  assert.strictEqual(shown.hiddenSymbols, 0);
  assert.ok(shown.table.length > hidden.table.length);
});

test('program output mixed into the stream is ignored', () => {
  const { objects, ignored } = parseLines('{"schema_version":1}\nname value region\n{broken\n[1]\n');
  assert.strictEqual(objects.length, 1);
  assert.strictEqual(ignored, 3);
});

test('refuses output without or with another schema_version', () => {
  assert.match(buildModel('hello').problems[0], /schema_version/);
  const other = buildModel('{"schema_version":2}');
  assert.ok(!other.ok);
  assert.match(other.problems[0], /Unsupported schema_version 2/);
});

test('exit status and signals mark the run as failed', () => {
  const failed = buildModel('{"schema_version":1}\n{"exit":{"status":3}}');
  assert.ok(failed.ok && failed.failed && failed.snapshot === null);
  const signaled = buildModel('{"schema_version":1}\n{"exit":{"signal":11}}');
  assert.ok(signaled.failed);
  const clean = buildModel('{"schema_version":1}\n{"exit":{"status":0}}');
  assert.ok(!clean.failed);
});

test('exec_failed is reported as an error and a failed run', () => {
  const model = buildModel('{"schema_version":1}\n{"error":"exec_failed","errno":2,"message":"No such file or directory"}');
  assert.ok(model.failed);
  assert.strictEqual(model.errors[0].error, 'exec_failed');
});

test('build failures from sys-sight run mark the run as failed', () => {
  for (const code of ['compile_failed', 'source_unreadable', 'unsupported_source', 'build_failed']) {
    const model = buildModel('{"schema_version":1}\n{"error":"' + code + '","message":"x"}');
    assert.ok(model.failed, code);
  }
  const notFatal = buildModel('{"schema_version":1}\n{"error":"maps_unreadable","message":"x"}');
  assert.ok(!notFatal.failed);
});

test('the build line of sys-sight run is kept', () => {
  const model = buildModel('{"schema_version":1}\n{"build":{"compiler":"cc","flags":[],"duration_ms":390}}');
  assert.strictEqual(model.build.compiler, 'cc');
  assert.strictEqual(model.build.duration_ms, 390);
  assert.strictEqual(buildModel(sample).build, null);
});

test('consecutive mappings of one library become one row', () => {
  const rows = groupRegions([
    { ...region(1n, 2n, 'mmap', '/lib/libc.so.6'), symbols: [], frames: [] },
    { ...region(2n, 3n, 'mmap', '/lib/libc.so.6'), symbols: [], frames: [] },
    { ...region(5n, 6n, 'mmap', ''), symbols: [], frames: [] },
    { ...region(6n, 7n, 'mmap', ''), symbols: [], frames: [] },
  ]);
  assert.strictEqual(rows.length, 3);  // libc once, the two anonymous mappings stay separate
  assert.strictEqual(rows[0].parts.length, 2);
  assert.strictEqual(rows[0].end, 3n);
});

test('touching segments of the program become one row, separated ones do not', () => {
  const rows = groupRegions([
    { ...region(1n, 2n, 'rodata', '/p'), symbols: [], frames: [] },
    { ...region(2n, 3n, 'rodata', '/p'), symbols: [], frames: [] },
    { ...region(5n, 6n, 'rodata', '/p'), symbols: [], frames: [] },
  ]);
  assert.strictEqual(rows.length, 2);
  assert.strictEqual(rows[0].end, 3n);
});

test('a pointer shows the symbol or region it points to', () => {
  const model = buildModel(stream({
    regions: [
      region('0x404000', '0x405000', 'data', '/p'),
      region('0x1000000', '0x1021000', 'heap', '[heap]'),
    ],
    executable: { path: '/p', pie: false, sections: [], symbols: [
      { name: 'target', address: '0x404010', size: 4, section: '.data', value: '0x5' },
      { name: 'to_symbol', address: '0x404020', size: 8, section: '.data', value: '0x404010' },
      { name: 'to_heap', address: '0x404028', size: 8, section: '.data', value: '0x1000010' },
      { name: 'to_nothing', address: '0x404030', size: 8, section: '.data', value: '0x0' },
    ] },
  }));
  const byName = Object.fromEntries(model.table.map((row) => [row.name, row]));
  assert.strictEqual(byName.to_symbol.pointsTo, 'target');
  assert.strictEqual(byName.to_heap.pointsTo, '0x1000010 (heap)');
  assert.strictEqual(byName.to_nothing.pointsTo, '');
  assert.strictEqual(byName.target.pointsTo, '');
});

test('the oracle result is kept, and absent in older output', () => {
  const oracle = { compared: true, matches: false, mismatch_count: 1,
                   mismatches: [{ start: '0x1000', end: '0x2000', reducer: 'rw-', proc: null }] };
  const model = buildModel(stream({ regions: [], executable: { symbols: [], sections: [] }, oracle }));
  assert.strictEqual(model.oracle.mismatch_count, 1);
  assert.strictEqual(buildModel(stream({ regions: [] })).oracle, null);
});

test('sizes are readable', () => {
  assert.strictEqual(sizeText(4n), '4 B');
  assert.strictEqual(sizeText(4096n), '4 KiB');
  assert.strictEqual(sizeText(1536n), '1.5 KiB');
});
