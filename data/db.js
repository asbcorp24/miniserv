const $ = (id) => document.getElementById(id);

function setStatus(text) {
  const el = $('statusText');
  if (el) el.textContent = text;
}

function showResult(data) {
  const box = $('dbResult');
  if (!box) return;

  if (typeof data === 'string') {
    box.textContent = data;
  } else {
    box.textContent = JSON.stringify(data, null, 2);
  }
}

function formatBytes(bytes) {
  const n = Number(bytes || 0);

  if (n < 1024) return `${n} Б`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} КБ`;

  return `${(n / 1024 / 1024).toFixed(2)} МБ`;
}

async function apiGet(url) {
  setStatus('Загрузка...');

  const res = await fetch(url, {
    cache: 'no-store'
  });

  const data = await res.json();

  if (!res.ok || data.ok === false) {
    throw new Error(data.error || 'Ошибка запроса');
  }

  setStatus('Готово');
  return data;
}

async function apiPostJson(url, body = {}) {
  setStatus('Выполнение...');

  const res = await fetch(url, {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json;charset=utf-8'
    },
    body: JSON.stringify(body)
  });

  const data = await res.json();

  if (!res.ok || data.ok === false) {
    throw new Error(data.error || 'Ошибка запроса');
  }

  setStatus('Готово');
  return data;
}

function handleError(err) {
  console.error(err);
  setStatus('Ошибка');
  showResult(err.message || String(err));
  alert(err.message || 'Ошибка');
}

async function loadDbInfo() {
  try {
    const info = await apiGet('/api/db/info');

    $('dbExists').textContent = info.db_exists ? 'есть' : 'нет';
    $('dbSize').textContent = formatBytes(info.db_size);

    $('dbBackup').textContent = info.backup_exists
      ? `есть, ${formatBytes(info.backup_size)}`
      : 'нет';

    $('dbFsUsage').textContent =
      `${formatBytes(info.used_bytes)} / ${formatBytes(info.total_bytes)}`;

    showResult(info);
  } catch (err) {
    handleError(err);
  }
}

function exportDb() {
  window.location.href = '/api/db/export';
}

function exportBackupDb() {
  window.location.href = '/api/db/export-backup';
}

async function importDb(e) {
  e.preventDefault();

  const fileInput = $('dbImportFile');
  const file = fileInput.files && fileInput.files[0];

  if (!file) {
    alert('Выберите файл базы .db или .sqlite');
    return;
  }

  const ok = confirm(
    'Импорт заменит текущую базу данных на ESP32.\n\n' +
    'Перед заменой старая база будет сохранена как backup.\n\n' +
    'Продолжить?'
  );

  if (!ok) return;

  try {
    setStatus('Импорт базы...');

    const form = new FormData();
    form.append('dbfile', file, file.name);

    const res = await fetch('/api/db/import', {
      method: 'POST',
      body: form
    });

    const data = await res.json();

    if (!res.ok || data.ok === false) {
      throw new Error(data.error || 'Ошибка импорта базы');
    }

    fileInput.value = '';

    showResult(data);
    await loadDbInfo();

    alert('База успешно импортирована');
  } catch (err) {
    handleError(err);
  } finally {
    setStatus('Готово');
  }
}

async function restoreBackupDb() {
  const ok = confirm(
    'Восстановить backup базы данных?\n\n' +
    'Текущая база будет заменена файлом /app_backup.db.\n\n' +
    'Продолжить?'
  );

  if (!ok) return;

  try {
    const data = await apiPostJson('/api/db/restore-backup', {});

    showResult(data);
    await loadDbInfo();

    alert('Backup восстановлен');
  } catch (err) {
    handleError(err);
  }
}

function bindEvents() {
  const topActions = document.querySelector('.top-actions');
  if (topActions && !document.getElementById('dbAdminLink')) {
    const link = document.createElement('a');
    link.id = 'dbAdminLink';
    link.className = 'btn btn-outline-info btn-sm';
    link.href = '/db-admin.html';
    link.textContent = 'SQL / Таблицы';
    topActions.insertBefore(link, $('dbRefreshBtn'));
  }

  $('dbRefreshBtn').addEventListener('click', loadDbInfo);
  $('dbExportBtn').addEventListener('click', exportDb);
  $('dbBackupExportBtn').addEventListener('click', exportBackupDb);
  $('dbImportForm').addEventListener('submit', importDb);
  $('dbRestoreBackupBtn').addEventListener('click', restoreBackupDb);
}

window.addEventListener('DOMContentLoaded', () => {
  bindEvents();
  loadDbInfo();
});
