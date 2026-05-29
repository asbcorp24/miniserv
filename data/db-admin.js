const state = {
  tables: [],
  currentTable: '',
  currentColumns: [],
  currentRows: [],
  currentPk: '',
  editingRow: null
};

const $ = (id) => document.getElementById(id);

function setStatus(text) {
  $('statusText').textContent = text;
}

function showToast(text) {
  $('toastText').textContent = text;
  bootstrap.Toast.getOrCreateInstance($('appToast'), { delay: 2500 }).show();
}

function esc(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

async function apiGet(url) {
  setStatus('Загрузка...');
  const res = await fetch(url, { cache: 'no-store' });
  const data = await res.json();
  if (!res.ok || data.ok === false) throw new Error(data.error || 'Ошибка запроса');
  setStatus('Готово');
  return data;
}

async function apiPost(url, body = {}) {
  setStatus('Выполнение...');
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json;charset=utf-8' },
    body: JSON.stringify(body)
  });
  const data = await res.json();
  if (!res.ok || data.ok === false) throw new Error(data.error || 'Ошибка запроса');
  setStatus('Готово');
  return data;
}

function handleError(err) {
  console.error(err);
  setStatus('Ошибка');
  showToast(err.message || 'Ошибка');
}

function renderTables() {
  const q = $('tableSearch').value.toLowerCase();
  const rows = state.tables
    .filter(x => `${x.name} ${x.row_count}`.toLowerCase().includes(q))
    .map(x => `
      <button class="db-table-item ${state.currentTable === x.name ? 'active' : ''}" type="button" onclick="loadTable('${esc(x.name)}')">
        <div><b>${esc(x.name)}</b></div>
        <div class="small text-muted">${x.row_count} строк</div>
      </button>
    `).join('');
  $('tablesList').innerHTML = rows || '<div class="text-muted">Таблиц не найдено</div>';
}

async function loadTables() {
  state.tables = await apiGet('/api/db/tables');
  renderTables();
  if (!state.currentTable && state.tables.length) {
    await loadTableByName(state.tables[0].name);
  }
}

function renderTableData() {
  $('currentTableTitle').textContent = state.currentTable || 'Таблица не выбрана';

  const meta = {
    table: state.currentTable,
    columns: state.currentColumns.map(c => ({
      name: c.name,
      type: c.type,
      pk: c.pk,
      notnull: c.notnull
    })),
    row_count_loaded: state.currentRows.length,
    primary_key: state.currentPk || null
  };
  $('tableMeta').textContent = JSON.stringify(meta, null, 2);

  $('tableDataHead').innerHTML = state.currentColumns.length
    ? `<tr>${state.currentColumns.map(c => `<th>${esc(c.name)}</th>`).join('')}<th></th></tr>`
    : '';

  $('tableDataBody').innerHTML = state.currentRows.map((row, index) => `
    <tr>
      ${state.currentColumns.map(c => `<td title="${esc(row[c.name])}">${esc(row[c.name])}</td>`).join('')}
      <td><button class="btn btn-sm btn-outline-info" type="button" onclick="selectRow(${index})">Изм.</button></td>
    </tr>
  `).join('') || '<tr><td colspan="99" class="text-muted">Нет строк</td></tr>';
}

function renderEditor() {
  const row = state.editingRow || {};
  $('rowEditor').innerHTML = state.currentColumns.map(c => `
    <div>
      <label class="form-label">${esc(c.name)}${c.pk ? ' [PK]' : ''}${Number(c.notnull) ? ' *' : ''}</label>
      <input
        class="form-control code-area"
        id="edit_${esc(c.name)}"
        value="${esc(row[c.name] ?? '')}"
        placeholder="${esc(c.type || '')}${Number(c.notnull) ? ' / required' : ''}"
        ${c.pk && row[c.name] !== undefined && row[c.name] !== '' ? 'readonly' : ''}
      >
    </div>
  `).join('') || '<div class="text-muted">Сначала выберите таблицу</div>';
}

async function loadTableByName(name) {
  state.currentTable = name;
  const limit = Number($('rowLimit').value || 100);
  const data = await apiGet(`/api/db/table?name=${encodeURIComponent(name)}&limit=${limit}`);
  state.currentColumns = data.columns || [];
  state.currentRows = data.rows || [];
  state.currentPk = (state.currentColumns.find(c => Number(c.pk) === 1) || {}).name || '';
  state.editingRow = null;
  renderTables();
  renderTableData();
  renderEditor();
}

function selectRow(index) {
  state.editingRow = { ...state.currentRows[index] };
  renderEditor();
}

function newRow() {
  state.editingRow = {};
  for (const col of state.currentColumns) state.editingRow[col.name] = '';
  renderEditor();
}

function collectEditorRow() {
  const row = {};
  for (const col of state.currentColumns) {
    const input = $(`edit_${col.name}`);
    row[col.name] = input ? input.value : '';
  }
  return row;
}

function validateEditorRow(row) {
  return state.currentColumns
    .filter(col => {
      if (Number(col.pk) === 1) return false;
      if (!Number(col.notnull)) return false;
      if ((col.default_value ?? '') !== '') return false;
      return String(row[col.name] ?? '').trim().length === 0;
    })
    .map(col => col.name);
}

async function saveRow() {
  if (!state.currentTable) {
    showToast('Сначала выберите таблицу');
    return;
  }

  const row = collectEditorRow();
  const missing = validateEditorRow(row);
  if (missing.length) {
    showToast(`Заполните обязательные поля: ${missing.join(', ')}`);
    return;
  }

  const res = await apiPost('/api/db/table/save', {
    table: state.currentTable,
    row
  });
  showToast(`Сохранено, строк: ${res.affected_rows || 0}`);
  await loadTableByName(state.currentTable);
}

async function deleteRow() {
  if (!state.currentTable || !state.currentPk) {
    showToast('Не удалось определить primary key');
    return;
  }

  const row = collectEditorRow();
  const pkValue = row[state.currentPk];
  if (!pkValue) {
    showToast('Для удаления нужен primary key');
    return;
  }

  if (!confirm(`Удалить запись ${state.currentPk}=${pkValue}?`)) return;

  const res = await apiPost('/api/db/table/delete', {
    table: state.currentTable,
    pk_name: state.currentPk,
    pk_value: pkValue
  });
  showToast(`Удалено, строк: ${res.affected_rows || 0}`);
  state.editingRow = null;
  await loadTableByName(state.currentTable);
}

async function runSql() {
  const sql = $('sqlInput').value.trim();
  if (!sql) {
    showToast('Введите SQL');
    return;
  }

  const res = await apiPost('/api/db/query', { sql });
  $('sqlResult').textContent = JSON.stringify(res, null, 2);
  showToast('SQL выполнен');

  await loadTables();
  if (state.currentTable) await loadTableByName(state.currentTable);
}

function bindEvents() {
  $('refreshTablesBtn').addEventListener('click', loadTables);
  $('tableSearch').addEventListener('input', renderTables);
  $('reloadCurrentTableBtn').addEventListener('click', () => state.currentTable && loadTableByName(state.currentTable));
  $('newRowBtn').addEventListener('click', newRow);
  $('saveRowBtn').addEventListener('click', () => saveRow().catch(handleError));
  $('deleteRowBtn').addEventListener('click', () => deleteRow().catch(handleError));
  $('runSqlBtn').addEventListener('click', () => runSql().catch(handleError));
}

window.selectRow = selectRow;
window.loadTable = (name) => loadTableByName(name).catch(handleError);

window.addEventListener('DOMContentLoaded', async () => {
  bindEvents();
  try {
    await loadTables();
  } catch (err) {
    handleError(err);
  }
});
