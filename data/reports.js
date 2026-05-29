let reportTemplates = [];
let currentReport = null;
let currentRenderedHtml = '';

function setStatus(text) {
  const el = document.getElementById('statusText');
  if (el) el.textContent = text;
}

function escapeHtml(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function safeFileName(value) {
  return String(value || 'report')
    .replace(/[\\/:*?"<>|]+/g, '_')
    .replace(/\s+/g, '_')
    .substring(0, 80);
}

function nowFileStamp() {
  const d = new Date();
  const p = n => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}`;
}

function downloadTextFile(filename, content, mimeType) {
  const blob = new Blob([content], { type: mimeType || 'text/plain;charset=utf-8' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

async function apiGet(url) {
  const res = await fetch(url, { cache: 'no-store' });
  const text = await res.text();
  try { return JSON.parse(text); }
  catch (e) { throw new Error(text || 'Ошибка JSON'); }
}

async function apiPost(url, data) {
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data || {})
  });
  const text = await res.text();
  try { return JSON.parse(text); }
  catch (e) { throw new Error(text || 'Ошибка JSON'); }
}

function insertAtCursor(textarea, text) {
  const start = textarea.selectionStart || 0;
  const end = textarea.selectionEnd || 0;
  const before = textarea.value.substring(0, start);
  const after = textarea.value.substring(end);
  textarea.value = before + text + after;
  const pos = start + text.length;
  textarea.focus();
  textarea.setSelectionRange(pos, pos);
}

function sortedKeys(obj) {
  return Object.keys(obj || {}).sort((a, b) => {
    const an = Number(a);
    const bn = Number(b);
    if (!Number.isNaN(an) && !Number.isNaN(bn)) return an - bn;
    return String(a).localeCompare(String(b));
  });
}

function renderNTable(ntable) {
  const keys = sortedKeys(ntable);
  if (!keys.length) return '<div class="text-muted">Обычных значений add_ntable() нет</div>';

  let html = '<table class="table table-bordered table-sm report-ntable">';
  html += '<thead><tr><th>Параметр</th><th>Значение</th></tr></thead><tbody>';
  keys.forEach(key => {
    html += `<tr><td>${escapeHtml(key)}</td><td>${escapeHtml(ntable[key])}</td></tr>`;
  });
  html += '</tbody></table>';
  return html;
}

function renderFullTable(tableName, tableData) {
  const rows = sortedKeys(tableData).map(k => ({ key: k, value: tableData[k] }));

  if (!rows.length) {
    return `<div class="text-muted">Нет данных таблицы ${escapeHtml(tableName)}</div>`;
  }

  let html = `<table class="table table-bordered table-sm report-full-table">`;
  html += `<thead><tr><th>№</th><th>${escapeHtml(tableName)}</th></tr></thead><tbody>`;
  rows.forEach(r => {
    html += `<tr><td>${escapeHtml(r.key)}</td><td>${escapeHtml(r.value)}</td></tr>`;
  });
  html += `</tbody></table>`;
  return html;
}

function renderAllReportData(data) {
  const ntable = data?.ntable || {};
  const tables = data?.tables || {};
  const tableNames = sortedKeys(tables);

  let html = '<div class="report-all-data">';
  html += '<h3>Общие значения</h3>';
  html += renderNTable(ntable);

  if (!tableNames.length) {
    html += '<h3>Таблицы измерений</h3><div class="text-muted">Таблиц add_value() нет</div>';
  } else {
    tableNames.forEach(name => {
      html += `<h3>${escapeHtml(name)}</h3>`;
      html += renderFullTable(name, tables[name]);
    });
  }

  html += '</div>';
  return html;
}

function renderReportTemplate(templateHtml, data) {
  let html = templateHtml || '';
  const ntable = data?.ntable || {};
  const tables = data?.tables || {};

  // Все данные отчёта: {{table:all}} — add_ntable() + все таблицы add_value()
  html = html.replace(/\{\{\s*table\s*:\s*all\s*\}\}/gi, function () {
    return renderAllReportData(data);
  });

  // Полная таблица: {{table:volt}} — все строки одной таблицы add_value("volt", row, value)
  html = html.replace(/\{\{\s*table\s*:\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\}\}/g, function (_, tableName) {
    return renderFullTable(tableName, tables[tableName] || {});
  });

  // Одна строка таблицы: {{volt[1]}}
  html = html.replace(/\{\{\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\[\s*(\d+)\s*\]\s*\}\}/g, function (_, tableName, row) {
    if (tables[tableName] && tables[tableName][row] !== undefined) {
      return escapeHtml(tables[tableName][row]);
    }
    return '';
  });

  // Обычное значение: {{amper}}
  html = html.replace(/\{\{\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\}\}/g, function (_, key) {
    if (ntable[key] !== undefined) {
      return escapeHtml(ntable[key]);
    }
    return '';
  });

  return html;
}

function sampleData() {
  return {
    ntable: {
      amper: '23',
      temperature: '25.4 C',
      status: 'Норма'
    },
    tables: {
      volt: {
        '1': '12.1 V',
        '2': '12.0 V',
        '3': '11.9 V'
      },
      amper_table: {
        '1': '22.9 A',
        '2': '23.0 A',
        '3': '23.1 A'
      }
    }
  };
}

function newTemplate() {
  document.getElementById('tplId').value = '';
  document.getElementById('tplName').value = 'Электрический отчёт';
  document.getElementById('tplTitle').value = 'Отчёт измерений';
  document.getElementById('tplHtml').value = `<h2>Отчёт измерений</h2>\n\n<p><b>Ток:</b> {{amper}}</p>\n<p><b>Температура:</b> {{temperature}}</p>\n\n<h3>Все данные отчёта</h3>\n{{table:all}}`;
}

function editTemplate(id) {
  const t = reportTemplates.find(x => Number(x.id) === Number(id));
  if (!t) return;
  document.getElementById('tplId').value = t.id;
  document.getElementById('tplName').value = t.name || '';
  document.getElementById('tplTitle').value = t.title || '';
  document.getElementById('tplHtml').value = t.template_html || '';
}

async function loadTemplates() {
  setStatus('Загрузка шаблонов...');
  reportTemplates = await apiGet('/api/reports/templates');
  const q = (document.getElementById('templateSearch')?.value || '').toLowerCase();
  const rows = reportTemplates.filter(t => !q || String(t.name || '').toLowerCase().includes(q));
  document.getElementById('templatesTable').innerHTML = rows.map(t => `
    <tr>
      <td>${t.id}</td>
      <td>${escapeHtml(t.name)}</td>
      <td>${escapeHtml(t.title || '')}</td>
      <td class="text-end">
        <button class="btn btn-sm btn-primary" onclick="editTemplate(${t.id})">Открыть</button>
        <button class="btn btn-sm btn-danger" onclick="deleteTemplate(${t.id})">Удалить</button>
      </td>
    </tr>
  `).join('');
  setStatus('Готово');
}

async function saveTemplate(ev) {
  ev.preventDefault();
  const id = Number(document.getElementById('tplId').value || 0);
  const name = document.getElementById('tplName').value.trim();
  const title = document.getElementById('tplTitle').value.trim();
  const templateHtml = document.getElementById('tplHtml').value;
  if (!name) return alert('Введите название шаблона');
  if (!templateHtml) return alert('Введите HTML шаблон');

  setStatus('Сохранение...');
  const res = await apiPost('/api/reports/templates/save', {
    id,
    name,
    title,
    template_html: templateHtml,
    editor_json: '',
    is_active: 1
  });
  if (!res.ok) throw new Error(res.error || 'Ошибка сохранения');
  document.getElementById('tplId').value = res.id || id;
  await loadTemplates();
  setStatus('Шаблон сохранён');
}

async function deleteTemplate(id) {
  if (!confirm('Удалить шаблон отчёта?')) return;
  const res = await apiPost('/api/reports/templates/delete', { id });
  if (!res.ok) throw new Error(res.error || 'Ошибка удаления');
  await loadTemplates();
}

function previewTemplate() {
  const html = document.getElementById('tplHtml').value;
  document.getElementById('previewBody').innerHTML = renderReportTemplate(html, sampleData());
  new bootstrap.Modal(document.getElementById('previewModal')).show();
}

async function loadCurrentReport() {
  setStatus('Получение текущего отчёта...');
  currentReport = await apiGet('/api/reports/current');
  document.getElementById('currentJson').textContent = JSON.stringify(currentReport, null, 2);

  if (!currentReport.ready) {
    currentRenderedHtml = '';
    document.getElementById('reportPreview').innerHTML = '<div class="text-muted">Активного отчёта в RAM нет</div>';
    setStatus('Активного отчёта нет');
    return;
  }

  currentRenderedHtml = renderReportTemplate(currentReport.template_html, currentReport.data);
  document.getElementById('reportPreview').innerHTML = currentRenderedHtml;
  setStatus('Отчёт сформирован в браузере');
}

async function clearCurrentReport() {
  if (!confirm('Очистить текущий отчёт из RAM?')) return;
  const res = await apiPost('/api/reports/current/clear', {});
  if (!res.ok) throw new Error(res.error || 'Ошибка очистки');
  await loadCurrentReport();
}

function getCurrentReportName() {
  return currentReport?.report_name || document.getElementById('tplName')?.value || 'report';
}

function buildStandaloneReportHtml() {
  const title = escapeHtml(getCurrentReportName());
  const body = currentRenderedHtml || document.getElementById('reportPreview').innerHTML || '';
  return `<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<title>${title}</title>
<style>
body{font-family:Arial,sans-serif;margin:24px;color:#111;background:#fff;}
table{border-collapse:collapse;width:100%;margin:12px 0;}
th,td{border:1px solid #444;padding:6px 8px;text-align:left;}
th{background:#eee;}
h1,h2,h3{margin-top:0;}
.report-meta{font-size:12px;color:#666;margin-bottom:16px;}
@media print{body{margin:12mm}.no-print{display:none!important}table{page-break-inside:auto}tr{page-break-inside:avoid;page-break-after:auto}thead{display:table-header-group}}
</style>
</head>
<body>
<div class="report-meta">Сформировано: ${escapeHtml(new Date().toLocaleString())}</div>
${body}
</body>
</html>`;
}

function printCurrentReport() {
  if (!currentRenderedHtml) return alert('Сначала нажмите «Получить текущий отчёт»');
  const w = window.open('', '_blank');
  if (!w) return alert('Браузер заблокировал окно печати');
  w.document.open();
  w.document.write(buildStandaloneReportHtml());
  w.document.close();
  w.focus();
  setTimeout(() => w.print(), 300);
}

function saveCurrentReportHtml() {
  if (!currentRenderedHtml) return alert('Сначала нажмите «Получить текущий отчёт»');
  const filename = `${safeFileName(getCurrentReportName())}_${nowFileStamp()}.html`;
  downloadTextFile(filename, buildStandaloneReportHtml(), 'text/html;charset=utf-8');
}

function saveCurrentReportJson() {
  if (!currentReport || !currentReport.ready) return alert('Сначала нажмите «Получить текущий отчёт»');
  const filename = `${safeFileName(getCurrentReportName())}_${nowFileStamp()}.json`;
  downloadTextFile(filename, JSON.stringify(currentReport, null, 2), 'application/json;charset=utf-8');
}

function addHeaderBlock() {
  const title = prompt('Текст заголовка', 'Отчёт измерений');
  if (!title) return;
  insertAtCursor(document.getElementById('tplHtml'), `\n<h2>${escapeHtml(title)}</h2>\n`);
}

function addTextBlock() {
  const text = prompt('Текст блока', 'Описание отчёта');
  if (!text) return;
  insertAtCursor(document.getElementById('tplHtml'), `\n<p>${escapeHtml(text)}</p>\n`);
}

function addNtableBlock() {
  const key = prompt('Имя значения из add_ntable("имя", "значение")', 'amper');
  if (!key) return;
  const label = prompt('Подпись', key) || key;
  insertAtCursor(document.getElementById('tplHtml'), `\n<p><b>${escapeHtml(label)}:</b> {{${key}}}</p>\n`);
}

function addTableBlock() {
  const table = prompt('Имя таблицы из add_value("имя", row, value)', 'volt');
  if (!table) return;
  const count = Math.max(1, Math.min(50, Number(prompt('Количество строк', '3') || 3)));
  let html = `\n<table class="table table-bordered table-sm">\n  <thead><tr><th>№</th><th>${escapeHtml(table)}</th></tr></thead>\n  <tbody>\n`;
  for (let i = 1; i <= count; i++) {
    html += `    <tr><td>${i}</td><td>{{${table}[${i}]}}</td></tr>\n`;
  }
  html += '  </tbody>\n</table>\n';
  insertAtCursor(document.getElementById('tplHtml'), html);
}

function addFullTableBlock() {
  const table = prompt('Имя таблицы из add_value("имя", row, value)', 'volt');
  if (!table) return;
  const title = prompt('Заголовок таблицы', table) || table;
  insertAtCursor(document.getElementById('tplHtml'), `\n<h3>${escapeHtml(title)}</h3>\n{{table:${table}}}\n`);
}

function addAllDataBlock() {
  insertAtCursor(document.getElementById('tplHtml'), '\n<h3>Все данные отчёта</h3>\n{{table:all}}\n');
}

function addLineBlock() {
  insertAtCursor(document.getElementById('tplHtml'), '\n<hr>\n');
}

function bindEvents() {
  document.getElementById('templateForm').addEventListener('submit', saveTemplate);
  document.getElementById('btnNewTemplate').addEventListener('click', newTemplate);
  document.getElementById('btnReload').addEventListener('click', loadTemplates);
  document.getElementById('btnPreviewTemplate').addEventListener('click', previewTemplate);
  document.getElementById('btnLoadCurrent').addEventListener('click', loadCurrentReport);
  document.getElementById('btnClearCurrent').addEventListener('click', clearCurrentReport);
  document.getElementById('btnPrintReport').addEventListener('click', printCurrentReport);
  document.getElementById('btnSaveReportHtml').addEventListener('click', saveCurrentReportHtml);
  document.getElementById('btnSaveReportJson').addEventListener('click', saveCurrentReportJson);
  document.getElementById('addHeader').addEventListener('click', addHeaderBlock);
  document.getElementById('addText').addEventListener('click', addTextBlock);
  document.getElementById('addNtable').addEventListener('click', addNtableBlock);
  document.getElementById('addTable').addEventListener('click', addTableBlock);
  document.getElementById('addFullTable').addEventListener('click', addFullTableBlock);
  document.getElementById('addAllData').addEventListener('click', addAllDataBlock);
  document.getElementById('addLine').addEventListener('click', addLineBlock);
  document.getElementById('templateSearch').addEventListener('input', loadTemplates);
}

window.addEventListener('DOMContentLoaded', async () => {
  bindEvents();
  newTemplate();
  try { await loadTemplates(); }
  catch (e) { setStatus('Ошибка'); alert(e.message); }
});
