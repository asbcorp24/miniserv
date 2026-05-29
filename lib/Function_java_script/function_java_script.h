#ifndef FUNCTION_JAVA_SCRIPT_H_
#define FUNCTION_JAVA_SCRIPT_H_



#include <Arduino.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <ArduinoJson.h>
#include <WebServer_ESP32_W5500.h>
#include <function_java_script.h>
#include <Function_send_read.h>


//extern WebServer server(80);
//extern WebServer server;
//extern sqlite3* db = nullptr;


// // ======================= Helpers =======================
// String jsonEscape(const String& s) {
//   String out;
//   out.reserve(s.length() + 16);
//   for (size_t i = 0; i < s.length(); i++) {
//     char c = s[i];
//     switch (c) {
//       case '\\': out += "\\\\"; break;
//       case '"':  out += "\\\""; break;
//       case '\n': out += "\\n"; break;
//       case '\r': out += "\\r"; break;
//       case '\t': out += "\\t"; break;
//       default:
//         if ((uint8_t)c < 0x20) out += ' ';
//         else out += c;
//         break;
//     }
//   }
//   return out;
// }

// void sendJson(int code, const String& body) 
// {
//   server.sendHeader("Cache-Control", "no-store");
//   server.send(code, "application/json; charset=utf-8", body);
// }

// void sendOk() {
//   sendJson(200, "{\"ok\":true}");
// }

// void sendError(int code, const String& message) {
//   String body = "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
//   sendJson(code, body);
// }

// bool execSQL(const char* sql) {
//   char* errMsg = nullptr;
//   int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
//   if (rc != SQLITE_OK) {
//     Serial.print("SQL error: ");
//     Serial.println(errMsg ? errMsg : "unknown");
//     if (errMsg) sqlite3_free(errMsg);
//     return false;
//   }
//   return true;
// }

// bool parseJsonBody(DynamicJsonDocument& doc) {
//   if (!server.hasArg("plain")) {
//     sendError(400, "Нет JSON тела запроса");
//     return false;
//   }
//   DeserializationError err = deserializeJson(doc, server.arg("plain"));
//   if (err) {
//     sendError(400, "Ошибка JSON");
//     return false;
//   }
//   return true;
// }

// String colText(sqlite3_stmt* stmt, int col) {
//   const unsigned char* t = sqlite3_column_text(stmt, col);
//   return t ? String((const char*)t) : "";
// }

// int getIdArg() {
//   if (!server.hasArg("id")) return 0;
//   return server.arg("id").toInt();
// }

// bool bindText(sqlite3_stmt* stmt, int index, const String& value) {
//   return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
// }

// bool dbPrepare(sqlite3_stmt** stmt, const char* sql) {
//   int rc = sqlite3_prepare_v2(db, sql, -1, stmt, nullptr);
//   if (rc != SQLITE_OK) {
//     sendError(500, sqlite3_errmsg(db));
//     return false;
//   }
//   return true;
// }

// bool dbStepDone(sqlite3_stmt* stmt) {
//   int rc = sqlite3_step(stmt);
//   if (rc != SQLITE_DONE) {
//     String err = sqlite3_errmsg(db);
//     sqlite3_finalize(stmt);
//     sendError(500, err);
//     return false;
//   }
//   sqlite3_finalize(stmt);
//   return true;
// }

// // ======================= Database =======================
// bool initDatabase() {
//   int rc = sqlite3_open("/littlefs/app.db", &db);
//   if (rc != SQLITE_OK) {
//     Serial.print("Не удалось открыть SQLite базу: ");
//     Serial.println(sqlite3_errmsg(db));
//     return false;
//   }

//   execSQL("PRAGMA foreign_keys = ON;");

//   bool ok = true;

//   ok &= execSQL(
//     "CREATE TABLE IF NOT EXISTS devices ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
//     "name TEXT NOT NULL,"
//     "properties TEXT,"
//     "init_string TEXT"
//     ");"
//   );

//   ok &= execSQL(
//     "CREATE TABLE IF NOT EXISTS functions ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
//     "name TEXT NOT NULL,"
//     "comment TEXT"
//     ");"
//   );

//   ok &= execSQL(
//     "CREATE TABLE IF NOT EXISTS receive_functions ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
//     "device_id INTEGER NOT NULL,"
//     "function_id INTEGER NOT NULL,"
//     "get_code TEXT,"
//     "comment TEXT,"
//     "FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE,"
//     "FOREIGN KEY(function_id) REFERENCES functions(id) ON DELETE CASCADE"
//     ");"
//   );

//   ok &= execSQL(
//     "CREATE TABLE IF NOT EXISTS algorithms ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
//     "name TEXT NOT NULL,"
//     "algorithm_text TEXT"
//     ");"
//   );

//   ok &= execSQL(
//     "CREATE TABLE IF NOT EXISTS logs ("
//     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
//     "log_date TEXT NOT NULL,"
//     "log_time TEXT NOT NULL,"
//     "device_id INTEGER,"
//     "value TEXT,"
//     "direction TEXT CHECK(direction IN ('получение','установка')) NOT NULL,"
//     "FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE SET NULL"
//     ");"
//   );

//   return ok;
// }

// // ======================= Static files =======================
// void handleRoot() {
//   File file = LittleFS.open("/index.html", "r");
//   if (!file) {
//     server.send(404, "text/plain; charset=utf-8", "index.html не найден. Загрузи папку data через pio run -t uploadfs");
//     return;
//   }
//   server.streamFile(file, "text/html");
//   file.close();
// }

// void handleStaticFile(const char* path, const char* contentType) {
//   File file = LittleFS.open(path, "r");
//   if (!file) {
//     server.send(404, "text/plain", "File not found");
//     return;
//   }
//   server.streamFile(file, contentType);
//   file.close();
// }

// // ======================= Devices API =======================
// void handleGetDevices() {
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "SELECT id, name, properties, init_string FROM devices ORDER BY id DESC;")) return;

//   String json = "[";
//   bool first = true;
//   while (sqlite3_step(stmt) == SQLITE_ROW) {
//     if (!first) json += ",";
//     first = false;
//     json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
//     json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
//     json += ",\"properties\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
//     json += ",\"init_string\":\"" + jsonEscape(colText(stmt, 3)) + "\"}";
//   }
//   sqlite3_finalize(stmt);
//   json += "]";
//   sendJson(200, json);
// }

// void handleSaveDevice() {
//   DynamicJsonDocument doc(4096);
//   if (!parseJsonBody(doc)) return;

//   int id = doc["id"] | 0;
//   String name = doc["name"] | "";
//   String properties = doc["properties"] | "";
//   String initString = doc["init_string"] | "";
//   name.trim();
//   if (name.length() == 0) { sendError(400, "Название прибора пустое"); return; }

//   sqlite3_stmt* stmt = nullptr;
//   if (id > 0) {
//     if (!dbPrepare(&stmt, "UPDATE devices SET name=?, properties=?, init_string=? WHERE id=?;")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, properties);
//     bindText(stmt, 3, initString);
//     sqlite3_bind_int(stmt, 4, id);
//   } else {
//     if (!dbPrepare(&stmt, "INSERT INTO devices(name, properties, init_string) VALUES(?,?,?);")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, properties);
//     bindText(stmt, 3, initString);
//   }

//   if (dbStepDone(stmt)) sendOk();
// }

// void handleDeleteDevice() {
//   int id = getIdArg();
//   if (id <= 0) { sendError(400, "Не передан id"); return; }
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "DELETE FROM devices WHERE id=?;")) return;
//   sqlite3_bind_int(stmt, 1, id);
//   if (dbStepDone(stmt)) sendOk();
// }

// // ======================= Functions API =======================
// void handleGetFunctions() {
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "SELECT id, name, comment FROM functions ORDER BY id DESC;")) return;

//   String json = "[";
//   bool first = true;
//   while (sqlite3_step(stmt) == SQLITE_ROW) {
//     if (!first) json += ",";
//     first = false;
//     json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
//     json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
//     json += ",\"comment\":\"" + jsonEscape(colText(stmt, 2)) + "\"}";
//   }
//   sqlite3_finalize(stmt);
//   json += "]";
//   sendJson(200, json);
// }

// void handleSaveFunction() {
//   DynamicJsonDocument doc(2048);
//   if (!parseJsonBody(doc)) return;

//   int id = doc["id"] | 0;
//   String name = doc["name"] | "";
//   String comment = doc["comment"] | "";
//   name.trim();
//   if (name.length() == 0) { sendError(400, "Название функции пустое"); return; }

//   sqlite3_stmt* stmt = nullptr;
//   if (id > 0) {
//     if (!dbPrepare(&stmt, "UPDATE functions SET name=?, comment=? WHERE id=?;")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, comment);
//     sqlite3_bind_int(stmt, 3, id);
//   } else {
//     if (!dbPrepare(&stmt, "INSERT INTO functions(name, comment) VALUES(?,?);")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, comment);
//   }

//   if (dbStepDone(stmt)) sendOk();
// }

// void handleDeleteFunction() {
//   int id = getIdArg();
//   if (id <= 0) { sendError(400, "Не передан id"); return; }
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "DELETE FROM functions WHERE id=?;")) return;
//   sqlite3_bind_int(stmt, 1, id);
//   if (dbStepDone(stmt)) sendOk();
// }

// // ======================= Receive functions API =======================
// void handleGetReceiveFunctions() {
//   sqlite3_stmt* stmt = nullptr;
//   const char* sql =
//     "SELECT rf.id, rf.device_id, COALESCE(d.name,''), rf.function_id, COALESCE(f.name,''), rf.get_code, rf.comment "
//     "FROM receive_functions rf "
//     "LEFT JOIN devices d ON d.id = rf.device_id "
//     "LEFT JOIN functions f ON f.id = rf.function_id "
//     "ORDER BY rf.id DESC;";

//   if (!dbPrepare(&stmt, sql)) return;

//   String json = "[";
//   bool first = true;
//   while (sqlite3_step(stmt) == SQLITE_ROW) {
//     if (!first) json += ",";
//     first = false;
//     json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
//     json += ",\"device_id\":" + String(sqlite3_column_int(stmt, 1));
//     json += ",\"device_name\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
//     json += ",\"function_id\":" + String(sqlite3_column_int(stmt, 3));
//     json += ",\"function_name\":\"" + jsonEscape(colText(stmt, 4)) + "\"";
//     json += ",\"get_code\":\"" + jsonEscape(colText(stmt, 5)) + "\"";
//     json += ",\"comment\":\"" + jsonEscape(colText(stmt, 6)) + "\"}";
//   }
//   sqlite3_finalize(stmt);
//   json += "]";
//   sendJson(200, json);
// }

// void handleSaveReceiveFunction() {
//   DynamicJsonDocument doc(8192);
//   if (!parseJsonBody(doc)) return;

//   int id = doc["id"] | 0;
//   int deviceId = doc["device_id"] | 0;
//   int functionId = doc["function_id"] | 0;
//   String getCode = doc["get_code"] | "";
//   String comment = doc["comment"] | "";

//   if (deviceId <= 0) { sendError(400, "Не выбран прибор"); return; }
//   if (functionId <= 0) { sendError(400, "Не выбрана функция"); return; }

//   sqlite3_stmt* stmt = nullptr;
//   if (id > 0) {
//     if (!dbPrepare(&stmt, "UPDATE receive_functions SET device_id=?, function_id=?, get_code=?, comment=? WHERE id=?;")) return;
//     sqlite3_bind_int(stmt, 1, deviceId);
//     sqlite3_bind_int(stmt, 2, functionId);
//     bindText(stmt, 3, getCode);
//     bindText(stmt, 4, comment);
//     sqlite3_bind_int(stmt, 5, id);
//   } else {
//     if (!dbPrepare(&stmt, "INSERT INTO receive_functions(device_id, function_id, get_code, comment) VALUES(?,?,?,?);")) return;
//     sqlite3_bind_int(stmt, 1, deviceId);
//     sqlite3_bind_int(stmt, 2, functionId);
//     bindText(stmt, 3, getCode);
//     bindText(stmt, 4, comment);
//   }

//   if (dbStepDone(stmt)) sendOk();
// }

// void handleDeleteReceiveFunction() {
//   int id = getIdArg();
//   if (id <= 0) { sendError(400, "Не передан id"); return; }
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "DELETE FROM receive_functions WHERE id=?;")) return;
//   sqlite3_bind_int(stmt, 1, id);
//   if (dbStepDone(stmt)) sendOk();
// }

// // ======================= Algorithms API =======================
// void handleGetAlgorithms() {
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "SELECT id, name, algorithm_text FROM algorithms ORDER BY id DESC;")) return;

//   String json = "[";
//   bool first = true;
//   while (sqlite3_step(stmt) == SQLITE_ROW) {
//     if (!first) json += ",";
//     first = false;
//     json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
//     json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
//     json += ",\"algorithm_text\":\"" + jsonEscape(colText(stmt, 2)) + "\"}";
//   }
//   sqlite3_finalize(stmt);
//   json += "]";
//   sendJson(200, json);
// }

// void handleSaveAlgorithm() {
//   DynamicJsonDocument doc(8192);
//   if (!parseJsonBody(doc)) return;

//   int id = doc["id"] | 0;
//   String name = doc["name"] | "";
//   String algorithmText = doc["algorithm_text"] | "";
//   name.trim();
//   if (name.length() == 0) { sendError(400, "Название алгоритма пустое"); return; }

//   sqlite3_stmt* stmt = nullptr;
//   if (id > 0) {
//     if (!dbPrepare(&stmt, "UPDATE algorithms SET name=?, algorithm_text=? WHERE id=?;")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, algorithmText);
//     sqlite3_bind_int(stmt, 3, id);
//   } else {
//     if (!dbPrepare(&stmt, "INSERT INTO algorithms(name, algorithm_text) VALUES(?,?);")) return;
//     bindText(stmt, 1, name);
//     bindText(stmt, 2, algorithmText);
//   }

//   if (dbStepDone(stmt)) sendOk();
// }

// void handleDeleteAlgorithm() {
//   int id = getIdArg();
//   if (id <= 0) { sendError(400, "Не передан id"); return; }
//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "DELETE FROM algorithms WHERE id=?;")) return;
//   sqlite3_bind_int(stmt, 1, id);
//   if (dbStepDone(stmt)) sendOk();
// }

// // ======================= Logs API =======================
// void handleGetLogs() {
//   sqlite3_stmt* stmt = nullptr;
//   const char* sql =
//     "SELECT l.id, l.log_date, l.log_time, l.device_id, COALESCE(d.name,''), l.value, l.direction "
//     "FROM logs l "
//     "LEFT JOIN devices d ON d.id = l.device_id "
//     "ORDER BY l.id DESC LIMIT 300;";

//   if (!dbPrepare(&stmt, sql)) return;

//   String json = "[";
//   bool first = true;
//   while (sqlite3_step(stmt) == SQLITE_ROW) {
//     if (!first) json += ",";
//     first = false;
//     json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
//     json += ",\"log_date\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
//     json += ",\"log_time\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
//     json += ",\"device_id\":" + String(sqlite3_column_int(stmt, 3));
//     json += ",\"device_name\":\"" + jsonEscape(colText(stmt, 4)) + "\"";
//     json += ",\"value\":\"" + jsonEscape(colText(stmt, 5)) + "\"";
//     json += ",\"direction\":\"" + jsonEscape(colText(stmt, 6)) + "\"}";
//   }
//   sqlite3_finalize(stmt);
//   json += "]";
//   sendJson(200, json);
// }

// void handleAddLog() {
//   DynamicJsonDocument doc(2048);
//   if (!parseJsonBody(doc)) return;

//   int deviceId = doc["device_id"] | 0;
//   String value = doc["value"] | "";
//   String direction = doc["direction"] | "получение";
//   String logDate = doc["log_date"] | "";
//   String logTime = doc["log_time"] | "";

//   if (direction != "получение" && direction != "установка") {
//     sendError(400, "Направление должно быть: получение или установка");
//     return;
//   }

//   sqlite3_stmt* stmt = nullptr;
//   if (!dbPrepare(&stmt, "INSERT INTO logs(log_date, log_time, device_id, value, direction) VALUES(?,?,?,?,?);")) return;
//   bindText(stmt, 1, logDate);
//   bindText(stmt, 2, logTime);
//   if (deviceId > 0) sqlite3_bind_int(stmt, 3, deviceId);
//   else sqlite3_bind_null(stmt, 3);
//   bindText(stmt, 4, value);
//   bindText(stmt, 5, direction);

//   if (dbStepDone(stmt)) sendOk();
// }

// void handleClearLogs() {
//   if (!execSQL("DELETE FROM logs;")) {
//     sendError(500, "Ошибка очистки log");
//     return;
//   }
//   sendOk();
// }

// // ======================= Debug API =======================
// String nowDateString() {
//   struct tm timeinfo;
//   if (getLocalTime(&timeinfo, 50)) {
//     char buf[11];
//     strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
//     return String(buf);
//   }
//   return String(millis());
// }

// String nowTimeString() {
//   struct tm timeinfo;
//   if (getLocalTime(&timeinfo, 50)) {
//     char buf[9];
//     strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
//     return String(buf);
//   }
//   return String(millis());
// }

// void addDebugLog(int deviceId, const String& code, const String& source) {
//   sqlite3_stmt* stmt = nullptr;
//   if (sqlite3_prepare_v2(db, "INSERT INTO logs(log_date, log_time, device_id, value, direction) VALUES(?,?,?,?,?);", -1, &stmt, nullptr) != SQLITE_OK) {
//     Serial.print("Debug log SQL error: ");
//     Serial.println(sqlite3_errmsg(db));
//     return;
//   }

//   String date = nowDateString();
//   String time = nowTimeString();
//   String value = "DEBUG [" + source + "]: " + code;

//   bindText(stmt, 1, date);
//   bindText(stmt, 2, time);

//   if (deviceId > 0) sqlite3_bind_int(stmt, 3, deviceId);
//   else sqlite3_bind_null(stmt, 3);

//   bindText(stmt, 4, value);
//   bindText(stmt, 5, "установка");

//   int rc = sqlite3_step(stmt);
//   if (rc != SQLITE_DONE) {
//     Serial.print("Debug log insert error: ");
//     Serial.println(sqlite3_errmsg(db));
//   }

//   sqlite3_finalize(stmt);
// }

// void handleDebugStart() {
//   DynamicJsonDocument doc(8192);
//   if (!parseJsonBody(doc)) return;

//   String code = doc["code"] | "";
//   String source = doc["source"] | "manual";
//   int receiveId = doc["receive_id"] | 0;
//   int deviceId = doc["device_id"] | 0;
//   int functionId = doc["function_id"] | 0;

//   code.trim();

//   if (code.length() == 0) {
//     sendError(400, "Пустой код debug-команды");
//     return;
//   }

//   // ВАЖНО:
//   // ESP32 не может выполнить C++/Arduino-код, присланный строкой, как скрипт.
//   // Этот handler принимает код/команду, пишет в Serial и в log.
//   // Ниже потом можно добавить свой парсер команд: digitalWrite, read, init и т.д.

//   Serial.println("========== DEBUG START ==========");
//   Serial.print("source: ");
//   Serial.println(source);
//   Serial.print("receive_id: ");
//   Serial.println(receiveId);
//   Serial.print("device_id: ");
//   Serial.println(deviceId);
//   Serial.print("function_id: ");
//   Serial.println(functionId);
//   Serial.println("code:");
//   Serial.println(code);
//   Serial.println("=================================");

//   addDebugLog(deviceId, code, source);

//   String json = "{\"ok\":true";
//   json += ",\"source\":\"" + jsonEscape(source) + "\"";
//   json += ",\"receive_id\":" + String(receiveId);
//   json += ",\"device_id\":" + String(deviceId);
//   json += ",\"function_id\":" + String(functionId);
//   json += ",\"time\":\"" + jsonEscape(nowTimeString()) + "\"";
//   json += ",\"code\":\"" + jsonEscape(code) + "\"";
//   json += "}";

//   sendJson(200, json);
// }

#endif //FUNCTION_JAVA_SCRIPT_H_