// Turns sys-sight JSONL output into a view model. No DOM access, so it runs
// in the browser and under Node (see model.test.js).
(function (root) {
  'use strict';

  const SUPPORTED_VERSION = 1;

  // Errors after which there is no meaningful run to show.
  const RUN_FAILURES = new Set([
    'exec_failed', 'compile_failed', 'source_unreadable', 'unsupported_source', 'build_failed',
  ]);

  const TOOLCHAIN_SECTIONS = new Set([
    '.note.ABI-tag', '.eh_frame', '.init_array', '.fini_array', '.dynamic', '.got', '.got.plt',
    '.tm_clone_table', '.eh_frame_hdr',
  ]);

  function parseLines(text) {
    const objects = [];
    let ignored = 0;
    for (const raw of text.split('\n')) {
      const line = raw.trim();
      if (!line) continue;
      if (!line.startsWith('{')) { ignored++; continue; }  // output of the traced program
      try {
        const value = JSON.parse(line);
        if (value && typeof value === 'object' && !Array.isArray(value)) objects.push(value);
        else ignored++;
      } catch (error) {
        ignored++;
      }
    }
    return { objects, ignored };
  }

  const toBig = (text) => BigInt(text);
  const hex = (value) => '0x' + value.toString(16);

  function sizeText(bytes) {
    const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB'];
    let value = Number(bytes);
    let unit = 0;
    while (value >= 1024 && unit < units.length - 1) { value /= 1024; unit++; }
    return (Number.isInteger(value) ? String(value) : value.toFixed(1)) + ' ' + units[unit];
  }

  function isToolchainSymbol(symbol) {
    return symbol.name.startsWith('_') || /^completed\.\d+$/.test(symbol.name) ||
      TOOLCHAIN_SECTIONS.has(symbol.section);
  }

  function parseRegions(snapshot) {
    return (snapshot.regions || []).map((region) => ({
      start: toBig(region.start), end: toBig(region.end), kind: region.kind,
      permissions: region.permissions, name: region.name, symbols: [], frames: [],
    }));
  }

  // Consecutive mappings of one file become a single row: any run for
  // libraries, only touching segments for the program itself.
  function groupRegions(regions) {
    const rows = [];
    for (const region of regions) {
      const last = rows[rows.length - 1];
      if (last && region.kind === last.kind && region.name !== '' && region.name === last.name &&
          (region.kind === 'mmap' || region.start === last.end)) {
        last.parts.push(region);
        last.end = region.end;
      } else {
        rows.push({ kind: region.kind, name: region.name, start: region.start, end: region.end,
                    permissions: region.permissions, parts: [region], symbols: [], frames: [] });
      }
    }
    return rows;
  }

  function rowFor(rows, address) {
    return rows.find((row) => row.parts.some((part) => address >= part.start && address < part.end));
  }

  function pointsTo(symbol, symbols, rows) {
    if (symbol.size !== 8 || symbol.value === undefined) return '';
    const target = toBig(symbol.value);
    const named = symbols.find((other) => target >= toBig(other.address) &&
                                          target < toBig(other.address) + BigInt(Math.max(other.size, 1)));
    if (named) return named.name;
    const row = rowFor(rows, target);
    return row ? hex(target) + ' (' + row.kind + ')' : '';
  }

  function buildModel(text, options) {
    const showToolchain = Boolean(options && options.showToolchain);
    const { objects, ignored } = parseLines(text);
    const model = {
      ok: false, problems: [], ignoredLines: ignored, syscallCount: 0, diagnostics: [], errors: [],
      exit: null, failed: false, snapshot: null, rows: [], table: [], frames: [], hiddenSymbols: 0,
    };

    const versionLine = objects.find((object) => 'schema_version' in object);
    if (!versionLine) {
      model.problems.push('No schema_version line: this does not look like sys-sight output.');
      return model;
    }
    if (versionLine.schema_version !== SUPPORTED_VERSION) {
      model.problems.push('Unsupported schema_version ' + versionLine.schema_version +
                          ' (this viewer reads version ' + SUPPORTED_VERSION + ').');
      return model;
    }

    for (const object of objects) {
      if ('snapshot' in object) model.snapshot = object.snapshot;
      else if ('diagnostic' in object) model.diagnostics.push(object.diagnostic);
      else if ('error' in object) model.errors.push(object);
      else if ('exit' in object) model.exit = object.exit;
      else if ('sequence' in object && 'syscall' in object) model.syscallCount++;
    }
    model.build = (objects.find((object) => 'build' in object) || {}).build || null;
    model.failed = Boolean(model.exit && (model.exit.signal !== undefined || model.exit.status !== 0)) ||
                   model.errors.some((error) => RUN_FAILURES.has(error.error));
    model.ok = true;
    if (!model.snapshot) return model;

    const snapshot = model.snapshot;
    const rows = groupRegions(parseRegions(snapshot));
    const executable = snapshot.executable || { symbols: [], sections: [] };

    const visible = executable.symbols.filter((symbol) => showToolchain || !isToolchainSymbol(symbol));
    model.hiddenSymbols = executable.symbols.length - visible.length;
    for (const symbol of visible) {
      const row = rowFor(rows, toBig(symbol.address));
      if (row) row.symbols.push(symbol);
    }

    const frames = ((snapshot.stack && snapshot.stack.frames) || []).map((frame) => ({
      ...frame, startBig: toBig(frame.start), endBig: toBig(frame.end),
      sizeBytes: toBig(frame.end) - toBig(frame.start),
    }));
    for (const frame of frames) {
      const row = rowFor(rows, frame.startBig);
      if (row) row.frames.push(frame);
    }

    model.table = visible
      .map((symbol) => ({
        name: symbol.name,
        value: symbol.value === undefined ? '' : symbol.value,
        decimal: symbol.value === undefined ? '' : toBig(symbol.value).toString(),
        region: (rowFor(rows, toBig(symbol.address)) || { kind: '?' }).kind,
        section: symbol.section,
        address: symbol.address,
        pointsTo: pointsTo(symbol, visible, rows),
      }))
      .sort((a, b) => (toBig(a.address) < toBig(b.address) ? -1 : 1));

    model.rows = rows;
    model.frames = frames;
    model.stackEnd = snapshot.stack ? snapshot.stack.end : '';
    model.registers = snapshot.registers;
    model.executable = executable;
    return model;
  }

  const api = { SUPPORTED_VERSION, parseLines, buildModel, groupRegions, sizeText, hex, isToolchainSymbol };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.SysSightModel = api;
})(typeof window !== 'undefined' ? window : globalThis);
