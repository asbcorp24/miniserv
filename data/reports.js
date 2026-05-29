let reportTemplates = [];
let currentReport = null;

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

function renderReportTemplate(templateHtml, data) {
  let html = templateHtml || '';
  const ntable = data?.ntable || {};
  const tables = data?.tables || {};

  // Табличные значения: {{volt[1]}}
  html = html.replace(/\{\{\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\[\s*(\d+)\s*\]\s*\}\}/g, function (_, tableName, row) {
    if (tables[tableName] && tables[tableName][row] !== undefined) {
      return escapeHtml(tables[tableName][row]);
    }
    return '';
  });

  // Обычные значения: {{amper}}
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
      }
    }
  };
}

function newTemplate() {
  document.getElementById('tplId').value = '';
  document.getElementById('tplName').value = 'Электрический отчёт';
  document.getElementById('tplTitle').value = 'Отчёт измерений';
  document.getElementById('tplHtml').value = `<h2>Отчёт измерений</h2>\n\n<p><b>Ток:</b> {{amper}}</p>\n<p><b>Температура:</b> {{temperature}}</p>\n\n<table class="table table-bordered table-sm">\n  <thead>\n    <tr><th>№</th><th>Напряжение</th></tr>\n  </thead>\n  <tbody>\n    <tr><td>1</td><td>{{volt[1]}}</td></tr>\n    <tr><td>2</td><td>{{volt[2]}}</td></tr>\n    <tr><td>3</td><td>{{volt[3]}}</td></tr>\n  </tbody>\n</table>`;
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
    document.getElementById('reportPreview').innerHTML = '<div class="text-muted">Активного отчёта в RAM нет</div>';
    setStatus('Активного отчёта нет');
    return;
  }

  document.getElementById('reportPreview').innerHTML = renderReportTemplate(currentReport.template_html, currentReport.data);
  setStatus('Отчёт сформирован в браузере');
}

async function clearCurrentReport() {
  if (!confirm('Очистить текущий отчёт из RAM?')) return;
  const res = await apiPost('/api/reports/current/clear', {});
  if (!res.ok) throw new Error(res.error || 'Ошибка очистки');
  await loadCurrentReport();
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
  document.getElementById('addHeader').addEventListener('click', addHeaderBlock);
  document.getElementById('addText').addEventListener('click', addTextBlock);
  document.getElementById('addNtable').addEventListener('click', addNtableBlock);
  document.getElementById('addTable').addEventListener('click', addTableBlock);
  document.getElementById('addLine').addEventListener('click', addLineBlock);
  document.getElementById('templateSearch').addEventListener('input', loadTemplates);
}

window.addEventListener('DOMContentLoaded', async () => {
  bindEvents();
  newTemplate();
  try { await loadTemplates(); }
  catch (e) { setStatus('Ошибка'); alert(e.message); }
});
