#include "reports_module.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

extern WebServer server;
extern sqlite3* db;

#define REPORT_MAX_NTABLE_VALUES 64
#define REPORT_MAX_TABLE_VALUES 160
#define REPORT_MAX_NAME_LEN 40
#define REPORT_MAX_VALUE_LEN 160
#define REPORT_WS_PORT 81

struct ReportNTableValue { String key; String value; bool used; };
struct ReportTableValue { String table; int row; String value; bool used; };

struct ActiveReportState {
  bool ready;
  String reportName;
  int templateId;
  String templateHtml;
  String editorJson;
  unsigned long createdMs;
  uint32_t version;
  ReportNTableValue ntable[REPORT_MAX_NTABLE_VALUES];
  ReportTableValue tables[REPORT_MAX_TABLE_VALUES];
};

static ActiveReportState g_report;
static WebSocketsServer reportWs(REPORT_WS_PORT);
static bool reportWsStarted = false;
static uint32_t reportVersionCounter = 0;

static String reportJsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

static String colText(sqlite3_stmt* stmt, int col) {
  const unsigned char* t = sqlite3_column_text(stmt, col);
  return t ? String((const char*)t) : "";
}

static void sendReportJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}

static void sendReportError(int code, const String& msg) {
  sendReportJson(code, "{\"ok\":false,\"error\":\"" + reportJsonEscape(msg) + "\"}");
}

static bool parseReportJson(DynamicJsonDocument& doc) {
  if (!server.hasArg("plain")) {
    sendReportError(400, "Нет JSON тела запроса");
    return false;
  }
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    sendReportError(400, "Ошибка JSON");
    return false;
  }
  return true;
}

static bool prepareReport(sqlite3_stmt** stmt, const char* sql) {
  if (!db) {
    sendReportError(500, "SQLite не открыт");
    return false;
  }
  if (sqlite3_prepare_v2(db, sql, -1, stmt, nullptr) != SQLITE_OK) {
    sendReportError(500, sqlite3_errmsg(db));
    return false;
  }
  return true;
}

static bool bindText(sqlite3_stmt* stmt, int idx, const String& v) {
  return sqlite3_bind_text(stmt, idx, v.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

static String reportStatusEventJson(const char* type) {
  String json = "{\"type\":\"" + String(type) + "\"";
  json += ",\"ready\":" + String(g_report.ready ? "true" : "false");
  json += ",\"version\":" + String(g_report.version);
  json += ",\"template_id\":" + String(g_report.templateId);
  json += ",\"report_name\":\"" + reportJsonEscape(g_report.reportName) + "\"";
  json += ",\"created_ms\":" + String(g_report.createdMs);
  json += "}";
  return json;
}

static void reportBroadcastReady() {
  if (!reportWsStarted) return;
  String payload = reportStatusEventJson("report_ready");
  reportWs.broadcastTXT(payload);
}

static void reportWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    String answer = reportStatusEventJson(g_report.ready ? "report_ready" : "report_wait");
    reportWs.sendTXT(num, answer);
    return;
  }

  if (type == WStype_TEXT) {
    String msg;
    for (size_t i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();
    if (msg == "ping" || msg == "status") {
      String answer = reportStatusEventJson(g_report.ready ? "report_ready" : "report_wait");
      reportWs.sendTXT(num, answer);
    }
  }
}

void reportWebSocketLoop() {
  if (reportWsStarted) reportWs.loop();
}

static void clearReportValues() {
  for (int i = 0; i < REPORT_MAX_NTABLE_VALUES; i++) {
    g_report.ntable[i].key = "";
    g_report.ntable[i].value = "";
    g_report.ntable[i].used = false;
  }
  for (int i = 0; i < REPORT_MAX_TABLE_VALUES; i++) {
    g_report.tables[i].table = "";
    g_report.tables[i].row = 0;
    g_report.tables[i].value = "";
    g_report.tables[i].used = false;
  }
}

static void clearReportAll() {
  g_report.ready = false;
  g_report.reportName = "";
  g_report.templateId = 0;
  g_report.templateHtml = "";
  g_report.editorJson = "";
  g_report.createdMs = 0;
  g_report.version = reportVersionCounter;
  clearReportValues();
}

static String limitString(const String& s, int maxLen) {
  return ((int)s.length() <= maxLen) ? s : s.substring(0, maxLen);
}

static bool addNTableValue(const String& keyRaw, const String& valueRaw) {
  String key = keyRaw;
  key.trim();
  if (key.length() == 0) return false;
  key = limitString(key, REPORT_MAX_NAME_LEN);
  String value = limitString(valueRaw, REPORT_MAX_VALUE_LEN);

  for (int i = 0; i < REPORT_MAX_NTABLE_VALUES; i++) {
    if (g_report.ntable[i].used && g_report.ntable[i].key == key) {
      g_report.ntable[i].value = value;
      return true;
    }
  }

  for (int i = 0; i < REPORT_MAX_NTABLE_VALUES; i++) {
    if (!g_report.ntable[i].used) {
      g_report.ntable[i].key = key;
      g_report.ntable[i].value = value;
      g_report.ntable[i].used = true;
      return true;
    }
  }

  return false;
}

static bool addTableValue(const String& tableRaw, int row, const String& valueRaw) {
  String table = tableRaw;
  table.trim();
  if (table.length() == 0 || row <= 0) return false;
  table = limitString(table, REPORT_MAX_NAME_LEN);
  String value = limitString(valueRaw, REPORT_MAX_VALUE_LEN);

  for (int i = 0; i < REPORT_MAX_TABLE_VALUES; i++) {
    if (g_report.tables[i].used && g_report.tables[i].table == table && g_report.tables[i].row == row) {
      g_report.tables[i].value = value;
      return true;
    }
  }

  for (int i = 0; i < REPORT_MAX_TABLE_VALUES; i++) {
    if (!g_report.tables[i].used) {
      g_report.tables[i].table = table;
      g_report.tables[i].row = row;
      g_report.tables[i].value = value;
      g_report.tables[i].used = true;
      return true;
    }
  }

  return false;
}

static String valuesToJson() {
  String json = "{\"ntable\":{";
  bool first = true;

  for (int i = 0; i < REPORT_MAX_NTABLE_VALUES; i++) {
    if (!g_report.ntable[i].used) continue;
    if (!first) json += ",";
    first = false;
    json += "\"" + reportJsonEscape(g_report.ntable[i].key) + "\":\"" + reportJsonEscape(g_report.ntable[i].value) + "\"";
  }

  json += "},\"tables\":{";
  bool firstTable = true;

  for (int i = 0; i < REPORT_MAX_TABLE_VALUES; i++) {
    if (!g_report.tables[i].used) continue;

    String name = g_report.tables[i].table;
    bool done = false;
    for (int j = 0; j < i; j++) {
      if (g_report.tables[j].used && g_report.tables[j].table == name) done = true;
    }
    if (done) continue;

    if (!firstTable) json += ",";
    firstTable = false;
    json += "\"" + reportJsonEscape(name) + "\":{";

    bool firstRow = true;
    for (int k = 0; k < REPORT_MAX_TABLE_VALUES; k++) {
      if (!g_report.tables[k].used || g_report.tables[k].table != name) continue;
      if (!firstRow) json += ",";
      firstRow = false;
      json += "\"" + String(g_report.tables[k].row) + "\":\"" + reportJsonEscape(g_report.tables[k].value) + "\"";
    }

    json += "}";
  }

  json += "}}";
  return json;
}

static bool loadTemplate(const String& name, int& id, String& html, String& editor) {
  sqlite3_stmt* stmt = nullptr;
  const char* sql = "SELECT id, template_html, COALESCE(editor_json,'') FROM report_templates WHERE name=? AND is_active=1 LIMIT 1;";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

  bool ok = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    id = sqlite3_column_int(stmt, 0);
    html = colText(stmt, 1);
    editor = colText(stmt, 2);
    ok = true;
  }

  sqlite3_finalize(stmt);
  return ok;
}

static int makeActiveReport(const String& reportName) {
  int id = 0;
  String html;
  String editor;

  if (!loadTemplate(reportName, id, html, editor)) return 0;

  g_report.ready = true;
  g_report.reportName = reportName;
  g_report.templateId = id;
  g_report.templateHtml = html;
  g_report.editorJson = editor;
  g_report.createdMs = millis();
  g_report.version = ++reportVersionCounter;

  reportBroadcastReady();
  return id;
}

static int luaAddNTable(lua_State* L) {
  const char* key = luaL_checkstring(L, 1);
  const char* value = luaL_checkstring(L, 2);
  lua_pushboolean(L, addNTableValue(String(key), String(value)));
  return 1;
}

static int luaAddValue(lua_State* L) {
  const char* table = luaL_checkstring(L, 1);
  int row = luaL_checkinteger(L, 2);
  const char* value = luaL_checkstring(L, 3);
  lua_pushboolean(L, addTableValue(String(table), row, String(value)));
  return 1;
}

static int luaSendOtchet(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  int id = makeActiveReport(String(name));
  if (id > 0) lua_pushinteger(L, id);
  else lua_pushnil(L);
  return 1;
}

static int luaClearOtchet(lua_State* L) {
  clearReportAll();
  lua_pushboolean(L, true);
  return 1;
}

void registerReportLuaFunctions(lua_State* L) {
  lua_register(L, "add_ntable", luaAddNTable);
  lua_register(L, "add_value", luaAddValue);
  lua_register(L, "send_otchet", luaSendOtchet);
  lua_register(L, "clear_otchet", luaClearOtchet);
}

void initReportDatabase() {
  char* err = nullptr;
  const char* sql = "CREATE TABLE IF NOT EXISTS report_templates (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT UNIQUE NOT NULL,title TEXT,template_html TEXT NOT NULL,editor_json TEXT,is_active INTEGER DEFAULT 1,created_at TEXT DEFAULT CURRENT_TIMESTAMP,updated_at TEXT DEFAULT CURRENT_TIMESTAMP);";
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    Serial.print("[REPORT] init error: ");
    Serial.println(err ? err : "unknown");
    if (err) sqlite3_free(err);
  }
}

static void handleReportsPage() {
  File file = LittleFS.open("/reports.html", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "reports.html не найден");
    return;
  }
  server.streamFile(file, "text/html; charset=utf-8");
  file.close();
}

static void handleReportsJs() {
  File file = LittleFS.open("/reports.js", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "reports.js не найден");
    return;
  }
  server.streamFile(file, "application/javascript; charset=utf-8");
  file.close();
}

static void handleReportViewPage() {
  File file = LittleFS.open("/report-view.html", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "report-view.html не найден");
    return;
  }
  server.streamFile(file, "text/html; charset=utf-8");
  file.close();
}

static void handleReportViewJs() {
  File file = LittleFS.open("/report-view.js", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "report-view.js не найден");
    return;
  }
  server.streamFile(file, "application/javascript; charset=utf-8");
  file.close();
}

static void handleGetTemplates() {
  sqlite3_stmt* stmt = nullptr;
  if (!prepareReport(&stmt, "SELECT id,name,COALESCE(title,''),template_html,COALESCE(editor_json,''),is_active,created_at,updated_at FROM report_templates ORDER BY id DESC;")) return;

  String json = "[";
  bool first = true;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"name\":\"" + reportJsonEscape(colText(stmt, 1)) + "\"";
    json += ",\"title\":\"" + reportJsonEscape(colText(stmt, 2)) + "\"";
    json += ",\"template_html\":\"" + reportJsonEscape(colText(stmt, 3)) + "\"";
    json += ",\"editor_json\":\"" + reportJsonEscape(colText(stmt, 4)) + "\"";
    json += ",\"is_active\":" + String(sqlite3_column_int(stmt, 5));
    json += ",\"created_at\":\"" + reportJsonEscape(colText(stmt, 6)) + "\"";
    json += ",\"updated_at\":\"" + reportJsonEscape(colText(stmt, 7)) + "\"}";
  }

  sqlite3_finalize(stmt);
  json += "]";
  sendReportJson(200, json);
}

static void handleSaveTemplate() {
  DynamicJsonDocument doc(20000);
  if (!parseReportJson(doc)) return;

  int id = doc["id"] | 0;
  String name = doc["name"] | "";
  name.trim();
  String title = doc["title"] | "";
  String html = doc["template_html"] | "";
  String editor = doc["editor_json"] | "";
  int active = doc["is_active"] | 1;

  if (name.length() == 0) {
    sendReportError(400, "Название пустое");
    return;
  }
  if (html.length() == 0) {
    sendReportError(400, "HTML шаблон пустой");
    return;
  }

  sqlite3_stmt* stmt = nullptr;

  if (id > 0) {
    if (!prepareReport(&stmt, "UPDATE report_templates SET name=?,title=?,template_html=?,editor_json=?,is_active=?,updated_at=CURRENT_TIMESTAMP WHERE id=?;")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, title);
    bindText(stmt, 3, html);
    bindText(stmt, 4, editor);
    sqlite3_bind_int(stmt, 5, active ? 1 : 0);
    sqlite3_bind_int(stmt, 6, id);
  } else {
    if (!prepareReport(&stmt, "INSERT INTO report_templates(name,title,template_html,editor_json,is_active) VALUES(?,?,?,?,?);")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, title);
    bindText(stmt, 3, html);
    bindText(stmt, 4, editor);
    sqlite3_bind_int(stmt, 5, active ? 1 : 0);
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    String e = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sendReportError(500, e);
    return;
  }

  int savedId = id > 0 ? id : (int)sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  sendReportJson(200, "{\"ok\":true,\"id\":" + String(savedId) + "}");
}

static void handleDeleteTemplate() {
  DynamicJsonDocument doc(1024);
  if (!parseReportJson(doc)) return;

  int id = doc["id"] | 0;
  if (id <= 0) {
    sendReportError(400, "Не передан id");
    return;
  }

  sqlite3_stmt* stmt = nullptr;
  if (!prepareReport(&stmt, "DELETE FROM report_templates WHERE id=?;")) return;
  sqlite3_bind_int(stmt, 1, id);

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    String e = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sendReportError(500, e);
    return;
  }

  sqlite3_finalize(stmt);
  sendReportJson(200, "{\"ok\":true}");
}

static void handleCurrentReport() {
  if (!g_report.ready) {
    sendReportJson(200, "{\"ok\":true,\"ready\":false,\"version\":" + String(g_report.version) + "}");
    return;
  }

  String json = "{\"ok\":true,\"ready\":true";
  json += ",\"report_name\":\"" + reportJsonEscape(g_report.reportName) + "\"";
  json += ",\"template_id\":" + String(g_report.templateId);
  json += ",\"created_ms\":" + String(g_report.createdMs);
  json += ",\"version\":" + String(g_report.version);
  json += ",\"template_html\":\"" + reportJsonEscape(g_report.templateHtml) + "\"";
  json += ",\"editor_json\":\"" + reportJsonEscape(g_report.editorJson) + "\"";
  json += ",\"data\":" + valuesToJson() + "}";
  sendReportJson(200, json);
}

static void handleClearCurrent() {
  clearReportAll();
  sendReportJson(200, "{\"ok\":true}");
}

void registerReportHttpRoutes() {
  server.on("/reports.html", HTTP_GET, handleReportsPage);
  server.on("/reports.js", HTTP_GET, handleReportsJs);
  server.on("/report-view.html", HTTP_GET, handleReportViewPage);
  server.on("/report-view.js", HTTP_GET, handleReportViewJs);
  server.on("/api/reports/templates", HTTP_GET, handleGetTemplates);
  server.on("/api/reports/templates/save", HTTP_POST, handleSaveTemplate);
  server.on("/api/reports/templates/delete", HTTP_POST, handleDeleteTemplate);
  server.on("/api/reports/current", HTTP_GET, handleCurrentReport);
  server.on("/api/reports/current/clear", HTTP_POST, handleClearCurrent);

  if (!reportWsStarted) {
    reportWs.begin();
    reportWs.onEvent(reportWsEvent);
    reportWsStarted = true;
    Serial.println("[REPORT] WebSocket started on port " + String(REPORT_WS_PORT));
  }
}
