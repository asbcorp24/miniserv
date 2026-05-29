let currentReport = null;
let currentRenderedHtml = '';
let lastVersion = 0;
let ws = null;
let fallbackTimer = null;

function logLine(text) {
  const box = document.getElementById('logBox');
  const t = new Date().toLocaleTimeString();
  box.textContent = `[${t}] ${text}\n` + box.textContent;
}

function setStatus(text) {
  document.getElementById('statusText').textContent = text;
}

function setWsState(text, cls) {
  document.getElementById('wsText').textContent = text;
  const dot = document.getElementById('wsDot');
  dot.className = 'dot' + (cls ? ' ' + cls : '');
}

function escapeHtml(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function sortedKeys(obj) {
  return Object.keys(obj || {}).sort((a, b) => {
    const an = Number(a);
    const bn = Number(b);
    if (!Number.isNaN(an) && !Number.isNaN(bn)) return an - bn;
    return String(a).localeCompare(String(b));
  });
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

function renderFullTable(tableName, tableData) {
  const rows = sortedKeys(tableData).map(k => ({ key: k, value: tableData[k] }));
  if (!rows.length) return `<div style="color:#666">Нет данных таблицы ${escapeHtml(tableName)}</div>`;

  let html = '<table>';
  html += `<thead><tr><th>№</th><th>${escapeHtml(tableName)}</th></tr></thead><tbody>`;
  rows.forEach(r => {
    html += `<tr><td>${escapeHtml(r.key)}</td><td>${escapeHtml(r.value)}</td></tr>`;
  });
  html += '</tbody></table>';
  return html;
}

function renderAllAddValueTable(tables) {
  const tableNames = sortedKeys(tables);
  if (!tableNames.length) return '<div style="color:#666">Данных add_value() нет</div>';

  const rowSet = new Set();
  tableNames.forEach(name => sortedKeys(tables[name]).forEach(row => rowSet.add(row)));
  const rows = Array.from(rowSet).sort((a, b) => {
    const an = Number(a);
    const bn = Number(b);
    if (!Number.isNaN(an) && !Number.isNaN(bn)) return an - bn;
    return String(a).localeCompare(String(b));
  });

  let html = '<table>';
  html += '<thead><tr><th>№</th>';
  tableNames.forEach(name => html += `<th>${escapeHtml(name)}</th>`);
  html += '</tr></thead><tbody>';
  rows.forEach(row => {
    html += `<tr><td>${escapeHtml(row)}</td>`;
    tableNames.forEach(name => {
      const value = tables[name] && tables[name][row] !== undefined ? tables[name][row] : '';
      html += `<td>${escapeHtml(value)}</td>`;
    });
    html += '</tr>';
  });
  html += '</tbody></table>';
  return html;
}

function renderReportTemplate(templateHtml, data) {
  let html = templateHtml || '';
  const ntable = data?.ntable || {};
  const tables = data?.tables || {};

  html = html.replace(/\{\{\s*table\s*:\s*all\s*\}\}/gi, () => renderAllAddValueTable(tables));

  html = html.replace(/\{\{\s*table\s*:\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\}\}/g, (_, tableName) => {
    return renderFullTable(tableName, tables[tableName] || {});
  });

  html = html.replace(/\{\{\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\[\s*(\d+)\s*\]\s*\}\}/g, (_, tableName, row) => {
    if (tables[tableName] && tables[tableName][row] !== undefined) return escapeHtml(tables[tableName][row]);
    return '';
  });

  html = html.replace(/\{\{\s*([a-zA-Z0-9_а-яА-ЯёЁ.-]+)\s*\}\}/g, (_, key) => {
    if (ntable[key] !== undefined) return escapeHtml(ntable[key]);
    return '';
  });

  return html;
}

function showWaiting() {
  document.getElementById('waitBlock').style.display = 'block';
  document.getElementById('reportWrap').style.display = 'none';
}

function showReport(report) {
  currentReport = report;
  lastVersion = Number(report.version || 0);
  currentRenderedHtml = renderReportTemplate(report.template_html, report.data);

  document.getElementById('reportMeta').innerHTML =
    `Отчёт: <b>${escapeHtml(report.report_name || '')}</b> | ` +
    `template_id: ${escapeHtml(report.template_id || '')} | ` +
    `version: ${escapeHtml(report.version || '')} | ` +
    `получено: ${escapeHtml(new Date().toLocaleString())}`;

  document.getElementById('reportBody').innerHTML = currentRenderedHtml;
  document.getElementById('waitBlock').style.display = 'none';
  document.getElementById('reportWrap').style.display = 'block';
  setStatus('Отчёт показан');
}

async function loadCurrentReport(force) {
  try {
    const report = await apiGet('/api/reports/current');
    if (!report.ready) {
      if (force) showWaiting();
      setStatus('Ожидание send_otchet()...');
      return;
    }
    if (force || Number(report.version || 0) !== lastVersion) {
      showReport(report);
      logLine('Получен отчёт: ' + (report.report_name || '') + ', version=' + (report.version || 0));
    }
  } catch (e) {
    setStatus('Ошибка получения отчёта');
    logLine('Ошибка GET /api/reports/current: ' + e.message);
  }
}

function connectWs() {
  const url = `ws://${location.hostname}:81/`;
  try {
    ws = new WebSocket(url);
  } catch (e) {
    setWsState('WS ошибка, polling', 'bad');
    startFallbackPolling();
    return;
  }

  ws.onopen = () => {
    setWsState('WS подключен', 'ok');
    logLine('WebSocket подключен: ' + url);
    ws.send('status');
  };

  ws.onmessage = (ev) => {
    try {
      const msg = JSON.parse(ev.data);
      if (msg.type === 'report_ready' && msg.ready) {
        logLine('WS: report_ready version=' + (msg.version || 0));
        loadCurrentReport(true);
      } else {
        setStatus('Ожидание send_otchet()...');
      }
    } catch (e) {
      logLine('WS message: ' + ev.data);
    }
  };

  ws.onerror = () => {
    setWsState('WS ошибка', 'bad');
  };

  ws.onclose = () => {
    setWsState('WS отключен, polling', 'bad');
    logLine('WebSocket отключен, включён запасной polling');
    startFallbackPolling();
    setTimeout(connectWs, 3000);
  };
}

function startFallbackPolling() {
  if (fallbackTimer) return;
  fallbackTimer = setInterval(() => loadCurrentReport(false), 2000);
}

function buildStandaloneHtml() {
  const title = escapeHtml(currentReport?.report_name || 'report');
  return `<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><title>${title}</title>
<style>
body{font-family:Arial,sans-serif;margin:24px;color:#111;background:#fff;}
table{border-collapse:collapse;width:100%;margin:12px 0;}
th,td{border:1px solid #444;padding:6px 8px;text-align:left;}
th{background:#eee;} .meta{font-size:12px;color:#666;margin-bottom:16px;}
@media print{body{margin:12mm}table{page-break-inside:auto}tr{page-break-inside:avoid;page-break-after:auto}thead{display:table-header-group}}
</style></head><body>
<div class="meta">Сформировано: ${escapeHtml(new Date().toLocaleString())}</div>
${currentRenderedHtml}
</body></html>`;
}

function printReport() {
  if (!currentRenderedHtml) return alert('Отчёт ещё не готов');
  const w = window.open('', '_blank');
  if (!w) return alert('Браузер заблокировал окно печати');
  w.document.open();
  w.document.write(buildStandaloneHtml());
  w.document.close();
  w.focus();
  setTimeout(() => w.print(), 300);
}

function saveHtml() {
  if (!currentRenderedHtml) return alert('Отчёт ещё не готов');
  const file = `${safeFileName(currentReport?.report_name || 'report')}_${nowFileStamp()}.html`;
  downloadTextFile(file, buildStandaloneHtml(), 'text/html;charset=utf-8');
}

window.addEventListener('DOMContentLoaded', () => {
  document.getElementById('btnRefresh').addEventListener('click', () => loadCurrentReport(true));
  document.getElementById('btnPrint').addEventListener('click', printReport);
  document.getElementById('btnSaveHtml').addEventListener('click', saveHtml);

  showWaiting();
  loadCurrentReport(true);
  connectWs();
  startFallbackPolling();
});
