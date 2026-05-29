const state = {
  devices: [],
  functions: [],
  receiveFunctions: [],
  algorithms: [],
  logs: []
};

const $ = (id) => document.getElementById(id);

function ensureDynamicUi() {
  const mainTabs = $('mainTabs');
  if (mainTabs && !document.getElementById('dbAdminNavLink')) {
    const li = document.createElement('li');
    li.className = 'nav-item';
    li.innerHTML = '<a class="nav-link" id="dbAdminNavLink" href="/db-admin.html">SQL / Таблицы</a>';
    mainTabs.appendChild(li);
  }

  const receiveSearch = $('receiveSearch');
  const receivePanel = receiveSearch?.closest('.table-head')?.parentElement;
  if (receivePanel && !$('receiveDeviceFilter')) {
    const block = document.createElement('div');
    block.className = 'mb-3';
    block.innerHTML = '<label class="form-label">Фильтр по прибору</label><select class="form-select" id="receiveDeviceFilter"></select>';
    const tableResponsive = receivePanel.querySelector('.table-responsive');
    receivePanel.insertBefore(block, tableResponsive);
  }

  const algorithmName = $('algorithmName');
  if (algorithmName && !$('algorithmActive')) {
    const block = document.createElement('div');
    block.className = 'form-check mb-3';
    block.innerHTML = '<input class="form-check-input" type="checkbox" id="algorithmActive"><label class="form-check-label" for="algorithmActive">Активный автозапуск Lua по GPIO_NUM_17</label>';
    algorithmName.insertAdjacentElement('afterend', block);
  }
}

function setStatus(text) {
  $('statusText').textContent = text;
}

function showToast(text) {
  $('toastText').textContent = text;
  const toast = bootstrap.Toast.getOrCreateInstance($('appToast'), { delay: 2600 });
  toast.show();
}

function esc(value) {
  return String(value ?? '')
    .replaceAll('&', '&amp;')
    .replaceAll('<', '&lt;')
    .replaceAll('>', '&gt;')
    .replaceAll('"', '&quot;')
    .replaceAll("'", '&#039;');
}

function cut(value, len = 90) {
  const s = String(value ?? '');
  return s.length > len ? s.slice(0, len) + '…' : s;
}

async function apiGet(url) {
  setStatus('Загрузка...');
  const res = await fetch(url, { cache: 'no-store' });
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || 'Ошибка запроса');
  setStatus('Готово');
  return data;
}

async function apiPost(url, body = {}) {
  setStatus('Сохранение...');
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

async function loadAll() {
  try {
    await Promise.all([
      loadDevices(),
      loadFunctions(),
      loadAlgorithms()
    ]);
    await loadReceiveFunctions();
    await loadLogs();
    fillAllSelects();
  } catch (err) {
    handleError(err);
  }
}

// ===================== Devices =====================
async function loadDevices() {
  state.devices = await apiGet('/api/devices');
  renderDevices();
  fillAllSelects();
}

function renderDevices() {
  const q = $('deviceSearch').value.toLowerCase();
  const rows = state.devices
    .filter(x => `${x.id} ${x.name} ${x.properties} ${x.init_string}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.name)}</b><div class="small text-muted">${esc(cut(x.init_string, 80))}</div></td>
              <td> ${esc(cut(x.ip, 80))}</td>
                  <td> ${esc(cut(x.port, 80))}</td>
        <td><div class="mini-code">${esc(cut(x.properties, 150))}</div></td>
        <td>
          <div class="action-buttons">
  <button class="btn btn-sm btn-success" onclick="startReceive(${x.id})">Старт</button>
            <button class="btn btn-sm btn-outline-info" onclick="editDevice(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteDevice(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');
  $('devicesTable').innerHTML = rows || `<tr><td colspan="4" class="text-muted">Приборов пока нет</td></tr>`;
}

function resetDeviceForm() {
  $('deviceId').value = '';
  $('deviceName').value = '';
  $('deviceProperties').value = '';
  $('deviceInit').value = '';
  $('deviceip').value = '';
}

function editDevice(id) {
  const x = state.devices.find(i => i.id === id);
  if (!x) return;
  $('deviceId').value = x.id;
  $('deviceName').value = x.name || '';
  $('deviceProperties').value = x.properties || '';
  $('deviceInit').value = x.init_string || '';
  $('deviceip').value = x.ip || '';
  $('port').value = x.port || '';
  document.querySelector('[data-bs-target="#tabDevices"]').click();
}

async function saveDevice(e) {
  e.preventDefault();
  try {
    await apiPost('/api/devices/save', {
      id: Number($('deviceId').value || 0),
      name: $('deviceName').value.trim(),
      properties: $('deviceProperties').value,
      ip:$('deviceip').value,
      port:$('port').value,
      init_string: $('deviceInit').value
    });
    resetDeviceForm();
    await loadDevices();
    await loadReceiveFunctions();
    await loadLogs();
    showToast('Прибор сохранён');
  } catch (err) { handleError(err); }
}

async function deleteDevice(id) {
  if (!confirm('Удалить прибор? Связанные функции получения тоже удалятся.')) return;
  try {
    await apiPost(`/api/devices/delete?id=${id}`);
    await loadDevices();
    await loadReceiveFunctions();
    await loadLogs();
    showToast('Прибор удалён');
  } catch (err) { handleError(err); }
}

// ===================== Functions =====================
async function loadFunctions() {
  state.functions = await apiGet('/api/functions');
  renderFunctions();
  fillAllSelects();
}

function renderFunctions() {
  const q = $('functionSearch').value.toLowerCase();
  const rows = state.functions
    .filter(x => `${x.id} ${x.name} ${x.comment}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.name)}</b></td>
        <td><div class="text-cut">${esc(x.comment)}</div></td>
        <td>
          <div class="action-buttons">
            <button class="btn btn-sm btn-outline-info" onclick="editFunction(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteFunction(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');
  $('functionsTable').innerHTML = rows || `<tr><td colspan="4" class="text-muted">Функций пока нет</td></tr>`;
}

function resetFunctionForm() {
  $('functionId').value = '';
  $('functionName').value = '';
  $('functionComment').value = '';
}

function editFunction(id) {
  const x = state.functions.find(i => i.id === id);
  if (!x) return;
  $('functionId').value = x.id;
  $('functionName').value = x.name || '';
  $('functionComment').value = x.comment || '';
  document.querySelector('[data-bs-target="#tabFunctions"]').click();
}

async function saveFunction(e) {
  e.preventDefault();
  try {
    await apiPost('/api/functions/save', {
      id: Number($('functionId').value || 0),
      name: $('functionName').value.trim(),
      comment: $('functionComment').value
    });
    resetFunctionForm();
    await loadFunctions();
    await loadReceiveFunctions();
    showToast('Функция сохранена');
  } catch (err) { handleError(err); }
}

async function deleteFunction(id) {
  if (!confirm('Удалить функцию? Связанные функции получения тоже удалятся.')) return;
  try {
    await apiPost(`/api/functions/delete?id=${id}`);
    await loadFunctions();
    await loadReceiveFunctions();
    showToast('Функция удалена');
  } catch (err) { handleError(err); }
}

// ===================== Receive functions =====================
async function loadReceiveFunctions() {
  state.receiveFunctions = await apiGet('/api/receive-functions');
  renderReceiveFunctions();
}

function renderReceiveFunctions() {
  const q = $('receiveSearch').value.toLowerCase();
  const deviceFilter = Number($('receiveDeviceFilter')?.value || 0);
  const rows = state.receiveFunctions
    .filter(x => !deviceFilter || Number(x.device_id) === deviceFilter)
    .filter(x => `${x.id} ${x.device_name} ${x.function_name} ${x.get_code} ${x.comment}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.device_name)}</b><div class="small text-muted">ID: ${x.device_id}</div></td>
        <td>${esc(x.function_name)}<div class="small text-muted">ID: ${x.function_id}</div></td>
        <td><div class="mini-code">${esc(cut(x.get_code, 160))}</div></td>
        <td>
          <div class="action-buttons">
            <button class="btn btn-sm btn-outline-info" onclick="editReceive(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteReceive(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');
  $('receiveTable').innerHTML = rows || `<tr><td colspan="5" class="text-muted">Функций получения пока нет</td></tr>`;
}

function resetReceiveForm() {
  $('receiveId').value = '';
  $('receiveDevice').value = '';
  fillReceiveFunctionSelect();
  $('receiveCode').value = '';
  $('receiveComment').value = '';
}

function editReceive(id) {
  const x = state.receiveFunctions.find(i => i.id === id);
  if (!x) return;
  $('receiveId').value = x.id;
  $('receiveDevice').value = x.device_id || '';
  fillReceiveFunctionSelect(x.function_id || '');
  $('receiveFunction').value = x.function_id || '';
  $('receiveCode').value = x.get_code || '';
  $('receiveComment').value = x.comment || '';
  document.querySelector('[data-bs-target="#tabReceive"]').click();
}

async function saveReceive(e) {
  e.preventDefault();
  try {
    await apiPost('/api/receive-functions/save', {
      id: Number($('receiveId').value || 0),
      device_id: Number($('receiveDevice').value || 0),
      function_id: Number($('receiveFunction').value || 0),
      get_code: $('receiveCode').value,
      comment: $('receiveComment').value
    });
    resetReceiveForm();
    await loadReceiveFunctions();
    showToast('Функция получения сохранена');
  } catch (err) { handleError(err); }
}
async function startReceive(id) {
  const x = state.receiveFunctions.find(i => i.id === id);
  if (!x) return showToast('Функция получения не найдена');

  try {
    const res = await sendDebugCode({
      code: x.get_code || '',
      source: 'receive_function',
      receive_id: x.id,
      device_id: x.device_id || 0,
      function_id: x.function_id || 0
    });

    showDebugResult(res);
    await loadLogs();
    showToast('Код функции получения отправлен');
  } catch (err) {
    handleError(err);
  }
}

// ===================== Debug =====================
async function sendDebugCode(payload) {
  return await apiPost('/api/debug/start', payload);
}

function showDebugResult(res) {
  const box = $('debugResult');
  if (!box) return;

  const lines = [
    `ok: ${res.ok === true}`,
    `source: ${res.source || 'manual'}`,
    `receive_id: ${res.receive_id || 0}`,
    `device_id: ${res.device_id || 0}`,
    `time: ${res.time || ''}`,
    '',
    'Отправленный код:',
    res.code || ''
  ];

  box.textContent = lines.join('\n');
}

function resetDebugForm() {
  $('debugCode').value = '';
  showDebugResult({
    ok: true,
    source: 'manual',
    code: ''
  });
}

async function startDebug(e) {
  e.preventDefault();

  const code = $('debugCode').value.trim();

  if (!code) {
    showToast('Введите код для отправки');
    return;
  }

  try {
    const res = await sendDebugCode({
      code,
      source: 'manual',
      receive_id: 0,
      device_id: 0,
      function_id: 0
    });

    showDebugResult(res);
    await loadLogs();
    showToast('Debug-команда отправлена');
  } catch (err) {
    handleError(err);
  }
}
async function deleteReceive(id) {
  if (!confirm('Удалить функцию получения?')) return;
  try {
    await apiPost(`/api/receive-functions/delete?id=${id}`);
    await loadReceiveFunctions();
    showToast('Функция получения удалена');
  } catch (err) { handleError(err); }
}

// ===================== Algorithms =====================
async function loadAlgorithms() {
  state.algorithms = await apiGet('/api/algorithms');
  renderAlgorithms();
}

function renderAlgorithms() {
  const q = $('algorithmSearch').value.toLowerCase();
  const table = $('algorithmsTable')?.closest('table');
  const headRow = table?.querySelector('thead tr');
  if (headRow && headRow.children.length === 4) {
    headRow.innerHTML = '<th>ID</th><th>Название</th><th>Активен</th><th>Алгоритм</th><th></th>';
  }

  const rows = state.algorithms
    .filter(x => `${x.id} ${x.name} ${x.algorithm_text}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.name)}</b></td>
        <td>${x.is_active ? '<span class="badge text-bg-success">Да</span>' : '<span class="badge text-bg-secondary">Нет</span>'}</td>
        <td><div class="mini-code">${esc(cut(x.algorithm_text, 200))}</div></td>
        <td>
          <div class="action-buttons">
            <button class="btn btn-sm btn-success" onclick="runAlgorithm(${x.id})">Пуск Lua</button>
            <button class="btn btn-sm btn-outline-info" onclick="editAlgorithm(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteAlgorithm(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');

  $('algorithmsTable').innerHTML = rows || `<tr><td colspan="4" class="text-muted">Алгоритмов пока нет</td></tr>`;
}

function resetAlgorithmForm() {
  $('algorithmId').value = '';
  $('algorithmName').value = '';
  if ($('algorithmActive')) $('algorithmActive').checked = false;
  $('algorithmText').value = '';
}

function editAlgorithm(id) {
  const x = state.algorithms.find(i => i.id === id);
  if (!x) return;
  $('algorithmId').value = x.id;
  $('algorithmName').value = x.name || '';
  if ($('algorithmActive')) $('algorithmActive').checked = !!x.is_active;
  $('algorithmText').value = x.algorithm_text || '';
  document.querySelector('[data-bs-target="#tabAlgorithms"]').click();
}

async function saveAlgorithm(e) {
  e.preventDefault();
  try {
    await apiPost('/api/algorithms/save', {
      id: Number($('algorithmId').value || 0),
      name: $('algorithmName').value.trim(),
      is_active: $('algorithmActive')?.checked ? 1 : 0,
      algorithm_text: $('algorithmText').value
    });
    resetAlgorithmForm();
    await loadAlgorithms();
    showToast('Алгоритм сохранён');
  } catch (err) { handleError(err); }
}

async function deleteAlgorithm(id) {
  if (!confirm('Удалить алгоритм?')) return;
  try {
    await apiPost(`/api/algorithms/delete?id=${id}`);
    await loadAlgorithms();
    showToast('Алгоритм удалён');
  } catch (err) { handleError(err); }
}
async function runAlgorithm(id) {
  const x = state.algorithms.find(i => i.id === id);

  if (!x) {
    showToast('Алгоритм не найден');
    return;
  }

  if (!confirm(`Запустить Lua-алгоритм "${x.name}"?`)) {
    return;
  }

  try {
    const res = await apiPost(`/api/algorithms/run?id=${id}`, {});

    showDebugResult({
      ok: res.ok,
      source: 'algorithm',
      receive_id: 0,
      device_id: 0,
      time: res.time || '',
      code:
        `Алгоритм: ${res.name}\n` +
        `ID: ${res.algorithm_id}\n\n` +
        `--- OUTPUT ---\n${res.output || ''}\n\n` +
        `--- LUA CODE ---\n${res.code || ''}`
    });

    document.querySelector('[data-bs-target="#tabDebug"]').click();

    await loadLogs();
    showToast('Lua-алгоритм выполнен');
  } catch (err) {
    handleError(err);
  }
}
// ===================== Logs =====================
async function loadLogs() {
  state.logs = await apiGet('/api/logs');
  renderLogs();
}

function renderLogs() {
  const q = $('logSearch').value.toLowerCase();
  const rows = state.logs
    .filter(x => `${x.id} ${x.log_date} ${x.log_time} ${x.device_name} ${x.value} ${x.direction}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td>${esc(x.log_date)}</td>
        <td>${esc(x.log_time)}</td>
        <td>${esc(x.device_name || '—')}</td>
        <td><div class="mini-code">${esc(x.value)}</div></td>
        <td><span class="badge-direction ${x.direction === 'установка' ? 'badge-set' : 'badge-get'}">${esc(x.direction)}</span></td>
      </tr>`).join('');
  $('logsTable').innerHTML = rows || `<tr><td colspan="6" class="text-muted">Журнал пустой</td></tr>`;
}

function resetLogForm() {
  $('logDevice').value = '';
  $('logValue').value = '';
  $('logDirection').value = 'получение';
}

function dateTimeNowParts() {
  const d = new Date();
  const yyyy = d.getFullYear();
  const mm = String(d.getMonth() + 1).padStart(2, '0');
  const dd = String(d.getDate()).padStart(2, '0');
  const hh = String(d.getHours()).padStart(2, '0');
  const mi = String(d.getMinutes()).padStart(2, '0');
  const ss = String(d.getSeconds()).padStart(2, '0');
  return { date: `${yyyy}-${mm}-${dd}`, time: `${hh}:${mi}:${ss}` };
}

async function addLog(e) {
  e.preventDefault();
  try {
    const dt = dateTimeNowParts();
    await apiPost('/api/logs/add', {
      log_date: dt.date,
      log_time: dt.time,
      device_id: Number($('logDevice').value || 0),
      value: $('logValue').value,
      direction: $('logDirection').value
    });
    resetLogForm();
    await loadLogs();
    showToast('Запись добавлена в log');
  } catch (err) { handleError(err); }
}

async function clearLogs() {
  if (!confirm('Очистить весь log?')) return;
  try {
    await apiPost('/api/logs/clear');
    await loadLogs();
    showToast('Log очищен');
  } catch (err) { handleError(err); }
}

// ===================== Selects =====================
function fillAllSelects() {
  fillDeviceSelect('receiveDevice', true);
  fillDeviceSelect('logDevice', true);
  fillFunctionSelect('receiveFunction', true);
}

function fillDeviceSelect(id, empty) {
  const select = $(id);
  if (!select) return;
  const old = select.value;
  select.innerHTML = (empty ? '<option value="">— выбери прибор —</option>' : '') +
    state.devices.map(x => `<option value="${x.id}">${esc(x.name)}</option>`).join('');
  if ([...select.options].some(o => o.value === old)) select.value = old;
}

function fillFunctionSelect(id, empty) {
  const select = $(id);
  if (!select) return;
  const old = select.value;
  select.innerHTML = (empty ? '<option value="">— выбери функцию —</option>' : '') +
    state.functions.map(x => `<option value="${x.id}">${esc(x.name)}</option>`).join('');
  if ([...select.options].some(o => o.value === old)) select.value = old;
}

// ===================== Events =====================
function bindEvents() {
  $('btnReloadAll').addEventListener('click', loadAll);

  $('deviceForm').addEventListener('submit', saveDevice);
  $('deviceReset').addEventListener('click', resetDeviceForm);
  $('deviceSearch').addEventListener('input', renderDevices);

  $('functionForm').addEventListener('submit', saveFunction);
  $('functionReset').addEventListener('click', resetFunctionForm);
  $('functionSearch').addEventListener('input', renderFunctions);

  $('receiveForm').addEventListener('submit', saveReceive);
  $('receiveReset').addEventListener('click', resetReceiveForm);
  $('receiveSearch').addEventListener('input', renderReceiveFunctions);

  $('algorithmForm').addEventListener('submit', saveAlgorithm);
  $('algorithmReset').addEventListener('click', resetAlgorithmForm);
  $('algorithmSearch').addEventListener('input', renderAlgorithms);

  $('logForm').addEventListener('submit', addLog);
  $('clearLogs').addEventListener('click', clearLogs);
  $('logSearch').addEventListener('input', renderLogs);
$('debugForm').addEventListener('submit', startDebug);
$('debugReset').addEventListener('click', resetDebugForm);
}

function fillAllSelects() {
  fillDeviceSelect('receiveDevice', true);
  fillDeviceSelect('receiveDeviceFilter', true, '— все приборы —');
  fillDeviceSelect('logDevice', true);
  fillReceiveFunctionSelect();
}

function fillDeviceSelect(id, empty, emptyLabel = 'вЂ” РІС‹Р±РµСЂРё РїСЂРёР±РѕСЂ вЂ”') {
  const select = $(id);
  if (!select) return;
  const old = select.value;
  select.innerHTML = (empty ? `<option value="">${emptyLabel}</option>` : '') +
    state.devices.map(x => `<option value="${x.id}">${esc(x.name)}</option>`).join('');
  if ([...select.options].some(o => o.value === old)) select.value = old;
}

function fillReceiveFunctionSelect(selectedValue = '') {
  fillFunctionSelect('receiveFunction', true);
  if (!selectedValue) return;
  const select = $('receiveFunction');
  if (select && [...select.options].some(o => o.value === String(selectedValue))) {
    select.value = String(selectedValue);
  }
}

function renderReceiveFunctions() {
  const q = $('receiveSearch').value.toLowerCase();
  const deviceFilter = Number($('receiveDeviceFilter')?.value || 0);
  const rows = state.receiveFunctions
    .filter(x => !deviceFilter || Number(x.device_id) === deviceFilter)
    .filter(x => `${x.id} ${x.device_name} ${x.function_name} ${x.get_code} ${x.comment}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.device_name)}</b><div class="small text-muted">ID: ${x.device_id}</div></td>
        <td>${esc(x.function_name)}<div class="small text-muted">ID: ${x.function_id}</div></td>
        <td><div class="mini-code">${esc(cut(x.get_code, 160))}</div></td>
        <td>
          <div class="action-buttons">
            <button class="btn btn-sm btn-outline-info" onclick="editReceive(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteReceive(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');
  $('receiveTable').innerHTML = rows || `<tr><td colspan="5" class="text-muted">Функций получения пока нет</td></tr>`;
}

function resetReceiveForm() {
  $('receiveId').value = '';
  $('receiveDevice').value = '';
  fillReceiveFunctionSelect();
  $('receiveCode').value = '';
  $('receiveComment').value = '';
}

function editReceive(id) {
  const x = state.receiveFunctions.find(i => i.id === id);
  if (!x) return;
  $('receiveId').value = x.id;
  $('receiveDevice').value = x.device_id || '';
  fillReceiveFunctionSelect(x.function_id || '');
  $('receiveFunction').value = x.function_id || '';
  $('receiveCode').value = x.get_code || '';
  $('receiveComment').value = x.comment || '';
  document.querySelector('[data-bs-target="#tabReceive"]').click();
}

function renderAlgorithms() {
  const q = $('algorithmSearch').value.toLowerCase();
  const table = $('algorithmsTable')?.closest('table');
  const headRow = table?.querySelector('thead tr');
  if (headRow && headRow.children.length === 4) {
    headRow.innerHTML = '<th>ID</th><th>Название</th><th>Активен</th><th>Алгоритм</th><th></th>';
  }

  const rows = state.algorithms
    .filter(x => `${x.id} ${x.name} ${x.algorithm_text}`.toLowerCase().includes(q))
    .map(x => `
      <tr>
        <td>${x.id}</td>
        <td><b>${esc(x.name)}</b></td>
        <td>${x.is_active ? '<span class="badge text-bg-success">Да</span>' : '<span class="badge text-bg-secondary">Нет</span>'}</td>
        <td><div class="mini-code">${esc(cut(x.algorithm_text, 200))}</div></td>
        <td>
          <div class="action-buttons">
            <button class="btn btn-sm btn-success" onclick="runAlgorithm(${x.id})">Пуск Lua</button>
            <button class="btn btn-sm btn-outline-info" onclick="editAlgorithm(${x.id})">Изм.</button>
            <button class="btn btn-sm btn-outline-danger" onclick="deleteAlgorithm(${x.id})">Удал.</button>
          </div>
        </td>
      </tr>`).join('');

  $('algorithmsTable').innerHTML = rows || `<tr><td colspan="5" class="text-muted">Алгоритмов пока нет</td></tr>`;
}

function resetAlgorithmForm() {
  $('algorithmId').value = '';
  $('algorithmName').value = '';
  if ($('algorithmActive')) $('algorithmActive').checked = false;
  $('algorithmText').value = '';
}

function editAlgorithm(id) {
  const x = state.algorithms.find(i => i.id === id);
  if (!x) return;
  $('algorithmId').value = x.id;
  $('algorithmName').value = x.name || '';
  if ($('algorithmActive')) $('algorithmActive').checked = !!x.is_active;
  $('algorithmText').value = x.algorithm_text || '';
  document.querySelector('[data-bs-target="#tabAlgorithms"]').click();
}

async function saveAlgorithm(e) {
  e.preventDefault();
  try {
    await apiPost('/api/algorithms/save', {
      id: Number($('algorithmId').value || 0),
      name: $('algorithmName').value.trim(),
      is_active: $('algorithmActive')?.checked ? 1 : 0,
      algorithm_text: $('algorithmText').value
    });
    resetAlgorithmForm();
    await loadAlgorithms();
    showToast('Алгоритм сохранён');
  } catch (err) { handleError(err); }
}

function bindEvents() {
  ensureDynamicUi();
  $('btnReloadAll').addEventListener('click', loadAll);

  $('deviceForm').addEventListener('submit', saveDevice);
  $('deviceReset').addEventListener('click', resetDeviceForm);
  $('deviceSearch').addEventListener('input', renderDevices);

  $('functionForm').addEventListener('submit', saveFunction);
  $('functionReset').addEventListener('click', resetFunctionForm);
  $('functionSearch').addEventListener('input', renderFunctions);

  $('receiveForm').addEventListener('submit', saveReceive);
  $('receiveReset').addEventListener('click', resetReceiveForm);
  $('receiveSearch').addEventListener('input', renderReceiveFunctions);
  $('receiveDevice').addEventListener('change', () => fillReceiveFunctionSelect());
  $('receiveDeviceFilter')?.addEventListener('change', renderReceiveFunctions);

  $('algorithmForm').addEventListener('submit', saveAlgorithm);
  $('algorithmReset').addEventListener('click', resetAlgorithmForm);
  $('algorithmSearch').addEventListener('input', renderAlgorithms);

  $('logForm').addEventListener('submit', addLog);
  $('clearLogs').addEventListener('click', clearLogs);
  $('logSearch').addEventListener('input', renderLogs);
  $('debugForm').addEventListener('submit', startDebug);
  $('debugReset').addEventListener('click', resetDebugForm);
}

window.addEventListener('DOMContentLoaded', () => {
  bindEvents();
  loadAll();
});
