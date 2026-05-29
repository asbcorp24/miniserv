#include "reports_module.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <WebServer_ESP32_W5500.h>

extern WebServer server;
extern sqlite3* db;

#define REPORT_MAX_NTABLE_VALUES 64
#define REPORT_MAX_TABLE_VALUES 160
#define REPORT_MAX_NAME_LEN 40
#define REPORT_MAX_VALUE_LEN 160

struct ReportNTableValue {
  String key;
  String value;
  bool used;
};

struct ReportTableValue {
  String table;
  int row;
  String value;
  bool used;
};

struct ActiveReportState {
  bool ready;
  String reportName;
  int templateId;
  String templateHtml;
  String editorJson;
  unsigned long createdMs;
  ReportNTableValue ntable[REPORT_MAX_NTABLE_VALUES];
  ReportTableValue tables[REPORT_MAX_TABLE_VALUES];
};

static ActiveReportState g_report;

static String reportJsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) out += ' ';
        else out += c;
        break;
    }
  }
  return out;
}

static void reportSendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}

static void reportSendError(int code, const String& message) {
  reportSendJson(code, "{\"ok\":false,\"error\":\"" + reportJsonEscape(message) + "\"}");
}

static String reportColText(sqlite3_stmt* stmt, int col) {
  const unsigned char* t = sqlite3_column_text(stmt, col);
  return t ? String((const char*)t) : "";
}

static bool reportExecSQL(const char* sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.print("[REPORT] SQL error: ");
    Serial.println(errMsg ? errMsg : "unknown");
    if (errMsg) sqlite3_free(errMsg);
    return false;
  }
  return true;
}

static bool reportPrepare(sqlite3_stmt** stmt, const char* sql) {
  if (!db) {
    reportSendError(500, "SQLite не открыт");
    return false;
  }
  int rc = sqlite3_prepare_v2(db, sql, -1, stmt, nullptr);
  if (rc != SQLITE_OK) {
    reportSendError(500, sqlite3_errmsg(db));
    return false;
  }
  return true;
}

static bool reportBindText(sqlite3_stmt* stmt, int index, const String& value) {
  return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

static bool reportParseJsonBody(DynamicJsonDocument& doc) {
  if (!server.hasArg("plain")) {
    reportSendError(400, "Нет JSON тела запроса");
    return false;
  }
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    reportSendError(400, "Ошибка JSON");
    return false;
  }
  return true;
}

static String reportLimitString(const String& src, int maxLen) {
  if ((int)src.length() <= maxLen) return src;
  return src.substring(0, maxLen);
}

static void reportClearValuesOnly() {
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

static void reportClearAll() {
  g_report.ready = false;
  g_report.reportName = "";
  g_report.templateId = 0;
  g_report.templateHtml = "";
  g_report.editorJson = "";
  g_report.createdMs = 0;
  reportClearValuesOnly();
}

static bool reportAddNTable(const String& keyRaw, const String& valueRaw) {
  String key = keyRaw;
  key.trim();
  if (key.length() == 0) return false;
  key = reportLimitString(key, REPORT_MAX_NAME_LEN);
  String value = reportLimitString(valueRaw, REPORT_MAX_VALUE_LEN);
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

static bool reportAddTableValue(const String& tableRaw, int row, const String& valueRaw) {
  String table = tableRaw;
  table.trim();
  if (table.length() == 0 || row <= 0) return false;
  table = reportLimitString(table, REPORT_MAX_NAME_LEN);
  String value = reportLimitString(valueRaw, REPORT_MAX_VALUE_LEN);
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

static String reportValuesJson() {
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
    String tableName = g_report.tables[i].table;
    bool done = false;
    for (int j = 0; j < i; j++) {
      if (g_report.tables[j].used && g_report.tables[j].table == tableName) done = true;
    }
    if (done) continue;
    if (!firstTable) json += ",";
    firstTable = false;
    json += "\"" + reportJsonEscape(tableName) + "\":{";
    bool firstRow = true;
    for (int k = 0; k < REPORT_MAX_TABLE_VALUES; k++) {
      if (!g_report.tables[k].used || g_report.tables[k].table != tableName) continue;
      if (!firstRow) json += ",";
      firstRow = false;
      json += "\"" + String(g_report.tables[k].row) + "\":\"" + reportJsonEscape(g_report.tables[k].value) + "\"";
    }
    json += "}";
  }
  json += "}}";
  return json;
}
