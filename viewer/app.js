// Renders the view model from model.js. All text goes through textContent, so
// names and paths from the output file can never inject markup.
(function () {
  'use strict';
  const M = window.SysSightModel;
  const byId = (id) => document.getElementById(id);

  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function add(parent, ...children) {
    for (const child of children) if (child) parent.appendChild(child);
    return parent;
  }

  function banner(kind, text) { return el('div', 'banner ' + kind, text); }

  function renderStatus(model) {
    const status = byId('status');
    status.replaceChildren();
    for (const problem of model.problems) status.appendChild(banner('error', problem));
    for (const error of model.errors) {
      status.appendChild(banner('error', error.error + (error.message ? ': ' + error.message : '')));
    }
    if (model.exit) {
      if (model.exit.signal !== undefined) {
        status.appendChild(banner('error', 'The program was killed by signal ' + model.exit.signal + '.'));
      } else if (model.exit.status !== 0) {
        status.appendChild(banner('error', 'The program exited with status ' + model.exit.status + '.'));
      } else if (model.snapshot) {
        status.appendChild(banner('ok', 'The program exited normally.'));
      }
    }
    for (const diagnostic of model.diagnostics) {
      status.appendChild(banner(diagnostic.level === 'error' ? 'error' : 'warn',
                                diagnostic.code + ': ' + diagnostic.message));
    }
    if (model.ok && !model.snapshot && !model.failed) {
      status.appendChild(banner('warn', 'The program never reached SYSSIGHT_SNAPSHOT(), so there is no snapshot.'));
    }
  }

  function summary(model) {
    const box = el('div', 'summary');
    const item = (label, value) => add(el('div'), el('span', null, label), el('span', 'mono', value));
    add(box, item('program', model.executable.path || '?'),
        item('rsp', model.registers.rsp), item('rbp', model.registers.rbp),
        item('pie', String(model.executable.pie)));
    return box;
  }

  function regionRow(row) {
    const node = el('div', 'row');
    node.dataset.kind = row.kind;
    const left = el('div');
    add(left, el('div', 'kind', row.kind), el('div', 'addr', M.hex(row.start)),
        el('div', 'addr', 'to ' + M.hex(row.end)));

    const body = el('div');
    const name = row.name || (row.kind === 'mmap' ? 'anonymous mapping' : row.kind);
    add(body, el('div', 'title', name),
        el('div', 'muted', M.sizeText(row.end - row.start) + ' · ' + row.permissions +
           (row.parts.length > 1 ? ' · ' + row.parts.length + ' mappings' : '')));

    if (row.parts.length > 1) {
      const details = el('details');
      const list = el('ul');
      for (const part of row.parts) {
        add(list, el('li', 'mono', M.hex(part.start) + ' – ' + M.hex(part.end) + '  ' + part.permissions));
      }
      add(details, el('summary', null, 'Show mappings'), list);
      body.appendChild(details);
    }
    if (row.symbols.length) {
      const chips = el('div', 'chips');
      for (const symbol of row.symbols) {
        const value = symbol.value === undefined ? '' : ' = ' + BigInt(symbol.value).toString();
        const chip = el('span', 'chip mono', symbol.name + value);
        chip.title = symbol.section + ' at ' + symbol.address + ', ' + symbol.size + ' bytes';
        chips.appendChild(chip);
      }
      body.appendChild(chips);
    }
    if (row.frames.length) {
      const frames = el('div', 'frames');
      row.frames.forEach((frame, index) => {
        const label = (index === 0 ? 'innermost · ' : '') + (frame.function || 'unknown function');
        const box = el('div', 'frame');
        add(box, el('strong', null, label),
            el('div', 'mono muted', M.hex(frame.startBig) + ' – ' + M.hex(frame.endBig) + ' · ' +
               M.sizeText(frame.sizeBytes) + ' · returns to ' + frame.return_address));
        frames.appendChild(box);
      });
      body.appendChild(frames);
    }
    add(node, left, body);
    return node;
  }

  function renderMap(model) {
    const map = el('div', 'map');
    model.rows.forEach((row, index) => {
      const previous = model.rows[index - 1];
      if (previous && row.start > previous.end) {
        map.appendChild(el('div', 'gap', '⋮ unmapped gap of ' + M.sizeText(row.start - previous.end)));
      }
      map.appendChild(regionRow(row));
    });
    return map;
  }

  function table(headers, rows) {
    const wrap = el('div', 'scroll');
    const node = el('table');
    const head = el('tr');
    for (const header of headers) {
      const cell = el('th', null, header);
      cell.scope = 'col';
      head.appendChild(cell);
    }
    node.appendChild(add(el('thead'), head));
    const body = el('tbody');
    for (const cells of rows) {
      const tr = el('tr');
      for (const cell of cells) tr.appendChild(el('td', 'mono', cell));
      body.appendChild(tr);
    }
    node.appendChild(body);
    wrap.appendChild(node);
    return wrap;
  }

  function render(text) {
    const model = M.buildModel(text, { showToolchain: byId('toolchain').checked });
    renderStatus(model);
    const result = byId('result');
    result.replaceChildren();
    if (!model.snapshot) return;

    add(result, el('h2', null, 'Snapshot'), summary(model));
    add(result, el('h2', null, 'Address space'),
        add(el('div', 'axis'), el('span', null, 'low addresses (top)'),
            el('span', null, 'high addresses (bottom)')),
        renderMap(model));

    add(result, el('h2', null, 'Variables'));
    if (model.table.length) {
      result.appendChild(table(['name', 'value', 'region', 'address', 'points to'],
        model.table.map((row) => [row.name, row.decimal === '' ? '' : row.decimal + ' (' + row.value + ')',
                                  row.region + ' ' + row.section, row.address, row.pointsTo])));
    } else {
      result.appendChild(el('p', 'muted', 'No global variables found (stripped or position-independent program?).'));
    }
    if (model.hiddenSymbols) {
      result.appendChild(el('p', 'muted', model.hiddenSymbols + ' toolchain symbols hidden.'));
    }

    add(result, el('h2', null, 'Stack frames'));
    if (model.frames.length) {
      result.appendChild(table(['#', 'function', 'start', 'end', 'size', 'frame pointer', 'returns to'],
        model.frames.map((frame, index) => [String(index), frame.function || '?', frame.start, frame.end,
                                            M.sizeText(frame.sizeBytes), frame.frame_pointer, frame.return_address])));
    } else {
      result.appendChild(el('p', 'muted', 'No frames. See the diagnostics above.'));
    }
    result.appendChild(el('p', 'muted', 'Frame walk ended: ' + model.stackEnd + ' · ' +
      model.syscallCount + ' system call events and ' + model.ignoredLines + ' other lines not shown.'));
  }

  let current = '';
  function show(text) { current = text; render(current); }

  byId('file').addEventListener('change', async (event) => {
    const file = event.target.files[0];
    if (file) show(await file.text());
  });
  byId('sample').addEventListener('click', () => show(window.SYSSIGHT_SAMPLE));
  byId('show').addEventListener('click', () => show(byId('paste').value));
  byId('toolchain').addEventListener('change', () => { if (current) render(current); });

  for (const type of ['dragenter', 'dragover']) {
    document.addEventListener(type, (event) => { event.preventDefault(); document.body.classList.add('dragging'); });
  }
  for (const type of ['dragleave', 'drop']) {
    document.addEventListener(type, (event) => { event.preventDefault(); document.body.classList.remove('dragging'); });
  }
  document.addEventListener('drop', async (event) => {
    const file = event.dataTransfer.files[0];
    if (file) show(await file.text());
  });

  if (location.hash === '#demo') show(window.SYSSIGHT_SAMPLE);
})();
