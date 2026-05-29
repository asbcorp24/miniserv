  //запрос на идентификаторы
  // "*idn?"

  //Rohde&Schwarz,SMB100A,1406.6000k03/184186,4.20.028.58
  //AnaPico AG,APULN40,1F1-3B5C00004-2252,0.4.194
  //Rohde&Schwarz,FSW-26,1331.5003K26/104358,6.20SP1
  //GW-INSTEK,PFR-100L,GEX220459,01.35.20240312
  //MX-2SP6T-0018
  //MX-2SP4T-0018

//////////////////////////////////////////////////////////
// изменения от 07.05.2026
//#include <Arduino.h>
//#include <WebServer.h>
#include <LittleFS.h>
#include <sqlite3.h>
#include <ArduinoJson.h>
#include <WebServer_ESP32_W5500.h>

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

extern "C" 
{
  #include "lua.h"
  #include "lualib.h"
  #include "lauxlib.h"
}


#define decimal_plase 3 //количество знаков после запятой для float

#define W5500_CS 5
#define NUMBER_OF_MAC 20


gpio_num_t  UZG   		= 	GPIO_NUM_15; 	//UZG
gpio_num_t  NGK   		= 	GPIO_NUM_16; 	//NGK
gpio_num_t  USER_debug_pin	= 	GPIO_NUM_17; 	//Режим пользователя / отладка

byte mac[NUMBER_OF_MAC][6] = {
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x02 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x03 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x04 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x05 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x06 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x07 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x08 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x09 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0A },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0B },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0C },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0D },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x0E },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x0F },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x10 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x11 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x12 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x13 },
  { 0xDE, 0xAD, 0xBE, 0xEF, 0xBE, 0x14 },
};

// Если твоя сеть не 192.168.2.x, поменяй IP/GW или закомментируй ETH.config(...)
IPAddress myIP(192, 168, 2, 232);
IPAddress myGW(192, 168, 2, 10);
IPAddress mySN(255, 255, 255, 0);
IPAddress myDNS(8, 8, 8, 8);

WebServer server(80);
sqlite3* db = nullptr;

SSD1306Wire display(0x3c, SDA, SCL);   // ADDRESS, SDA, SCL 


// ======================= DB import/export config =======================
File dbUploadFile;
bool dbUploadOk = false;
String dbUploadError = "";
size_t dbUploadSize = 0;

const char* DB_PATH = "/app.db";                  // путь внутри LittleFS
const char* DB_SQLITE_PATH = "/littlefs/app.db";  // путь для sqlite3_open()
const char* DB_BACKUP_PATH = "/app_backup.db";
const char* DB_IMPORT_TMP_PATH = "/app_import.tmp";


//WiFiClient *client_SMB100A = new WiFiClient();
WiFiClient *client_SM100A;
WiFiClient *client_RFSU40;
WiFiClient *client_FSW26; 
WiFiClient *client_PRF7100L;
WiFiClient *client_MX4;
WiFiClient *client_MX6;
WiFiClient *client;

typedef struct 
{
  String ID;
  WiFiClient *client;
	IPAddress IP_add; 
	uint16_t Port;
  bool identificator = false; //наличие ответа на запрос *IDN?
  String manufacture;
  String instrument;
  String ser_num;
  String soft_ver;
} IP_Port_Device;

IP_Port_Device MX_4;
IP_Port_Device MX_6;
IP_Port_Device Generator_get;
IP_Port_Device Generator;
IP_Port_Device DC_source;
IP_Port_Device Spectr_Analyzer;
IP_Port_Device Device;

IPAddress IP_RFSU40 (192, 168, 2, 101);
uint16_t port_RFSU40 = 18;
IPAddress IP_FSW26 (192, 168, 2, 15);
uint16_t port_FSW26 = 5025;
IPAddress IP_MSO_X_4054A (192, 168, 2, 30);
uint16_t port_MSO_X_4054A = 5025;
IPAddress IP_PRF7100L (192, 168, 2, 111);
uint16_t port_PRF7100L = 2268;
IPAddress IP_MX_2SP4T_0018 (192, 168, 2, 44);
uint16_t port_MX_2SP4T_0018 = 5000;
IPAddress IP_MX_2SP6T_0018 (192, 168, 2, 46);
uint16_t port_MX_2SP6T_0018 = 5000;
IPAddress IP_SMB100A (192, 168, 2, 100);
uint16_t port_SMB100A = 5025;

String send_IDN(IP_Port_Device *dev);
String send_IDN_MX(IP_Port_Device *dev);
String send_read(IP_Port_Device *dev , String send_mess);

void device_init();

void handleRunAlgorithm();
String runLuaAlgorithm(const String& code, bool& ok);
void runActiveAlgorithmsIfNeeded();

const unsigned long ACTIVE_ALGORITHM_INTERVAL_MS = 1000;
unsigned long lastActiveAlgorithmRunMs = 0;


static int lua_delay_ms(lua_State* L) 
{
  int   ms = luaL_checkinteger(L, 1);
  display.print("delay_ms ");
  display.println(ms);
  display.println(" start");
  vTaskDelay(ms);
  display.println(" stop");

  String val  = "delay_ms "  + String(ms)  + " Ok"; 
  lua_pushstring(L, val.c_str());
  return 1;
}



void UZG_attenuator_15dB() 
{ 
  digitalWrite(UZG, LOW);// 
}
void UZG_attenuator_0dB() 
{ 
  digitalWrite(UZG, HIGH);// 
}

void NGK_attenuator_30dB ()
{  
  digitalWrite(NGK, HIGH);//  
}
bool ipstringtobytes(String ip,unsigned char bytes[4]){
int a,b,c,d;
if(sscanf(ip.c_str(),"%d.%d.%d.%d",&a,&b,&c,&d)!=4){
  return false;
}
bytes[0]=static_cast<unsigned char>(a);
bytes[1]=static_cast<unsigned char>(a);
bytes[2]=static_cast<unsigned char>(a);
bytes[3]=static_cast<unsigned char>(a);
return true;
}
void NGK_attenuator_0dB ()
{  
  digitalWrite(NGK, LOW);//  
}

static int lua_NGK_30dB (lua_State* L)
{ 
  NGK_attenuator_30dB();
  Serial.println(" NGK_attenuator_30dB ");
  display.println(" NGK_attenuator_30dB "); 
  return 1;
}

static int lua_NGK_0dB (lua_State* L)
{ 
  NGK_attenuator_0dB();
  //Serial.println(" NGK_attenuator_0dB ");
  display.println(" NGK_attenuator_0dB ");

  //const char* cmd = luaL_checkstring(L, 1);

  String answer = "ESP32 record digitalWrite(NGK, LOW);";
  lua_pushstring(L, answer.c_str());
  //return LUA_OK;
  return 1;

}

static int lua_USER_mode (lua_State* L)
{ 
  String str = "";

  if(digitalRead(USER_debug_pin) == LOW)
  {
    str = "MODE_debug";    
  }
  else
  {
    str = "MODE_user";    
  } 
  display.println(str);
  lua_pushstring(L, str.c_str());                 //текстовое значение 
  lua_pushinteger(L, digitalRead(USER_debug_pin));//значение пина  
  return 2;
}


static int lua_UZG_0dB (lua_State* L)
{ 
  UZG_attenuator_0dB();
  Serial.println(" UZG_attenuator_0dB ");
  display.println(" UZG_attenuator_0dB ");
  return 1;
}

static int lua_UZG_15dB (lua_State* L)
{ 
  UZG_attenuator_15dB();
  Serial.println(" UZG_attenuator_15dB ");
  display.println(" UZG_attenuator_15dB ");
  return 1;
}



 

void PIN_OUT_INIT()
{
  pinMode(NGK, OUTPUT);
  pinMode(UZG, OUTPUT);
  pinMode(USER_debug_pin, INPUT_PULLUP); 
}

// ======================= Helpers =======================
String jsonEscape(const String& s) {
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
String nowTimeString() {
  unsigned long ms = millis();

  unsigned long sec = ms / 1000;
  unsigned long h = (sec / 3600) % 24;
  unsigned long m = (sec / 60) % 60;
  unsigned long s = sec % 60;

  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, s);

  return String(buf);
}
void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json; charset=utf-8", body);
}
void sendOk() {
  sendJson(200, "{\"ok\":true}");
}

void sendError(int code, const String& message) {
  String body = "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}";
  sendJson(code, body);
}

bool execSQL(const char* sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    Serial.print("SQL error: ");
    Serial.println(errMsg ? errMsg : "unknown");
    if (errMsg) sqlite3_free(errMsg);
    return false;
  }
  return true;
}

bool tableHasColumn(const char* tableName, const char* columnName) {
  String sql = "PRAGMA table_info(" + String(tableName) + ");";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.print("PRAGMA table_info error: ");
    Serial.println(sqlite3_errmsg(db));
    return false;
  }

  bool found = false;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const unsigned char* current = sqlite3_column_text(stmt, 1);
    if (current && String((const char*)current) == columnName) {
      found = true;
      break;
    }
  }
  sqlite3_finalize(stmt);
  return found;
}

bool parseJsonBody(DynamicJsonDocument& doc) {
  if (!server.hasArg("plain")) {
    sendError(400, "Нет JSON тела запроса");
    return false;
  }
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    sendError(400, "Ошибка JSON");
    return false;
  }
  return true;
}

String colText(sqlite3_stmt* stmt, int col) {
  const unsigned char* t = sqlite3_column_text(stmt, col);
  return t ? String((const char*)t) : "";
}

int getIdArg() {
  if (!server.hasArg("id")) return 0;
  return server.arg("id").toInt();
}

bool bindText(sqlite3_stmt* stmt, int index, const String& value) {
  return sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

bool dbPrepare(sqlite3_stmt** stmt, const char* sql) {
  int rc = sqlite3_prepare_v2(db, sql, -1, stmt, nullptr);
  if (rc != SQLITE_OK) {
    sendError(500, sqlite3_errmsg(db));
    return false;
  }
  return true;
}

bool dbStepDone(sqlite3_stmt* stmt) {
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    String err = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sendError(500, err);
    return false;
  }
  sqlite3_finalize(stmt);
  return true;
}

bool isSafeSqlIdentifier(const String& name) {
  if (name.length() == 0) return false;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    bool ok =
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '_';
    if (!ok) return false;
  }
  return true;
}

bool dbTableExists(const String& tableName) {
  if (!isSafeSqlIdentifier(tableName)) return false;

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;", -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }

  sqlite3_bind_text(stmt, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);
  bool exists = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return exists;
}

bool isWhitespaceOnly(const char* s) {
  if (!s) return true;
  while (*s) {
    char c = *s++;
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') return false;
  }
  return true;
}

String int64ToString(sqlite3_int64 value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)value);
  return String(buf);
}

String sqliteValueToJson(sqlite3_stmt* stmt, int col) {
  int type = sqlite3_column_type(stmt, col);
  switch (type) {
    case SQLITE_INTEGER:
      return int64ToString(sqlite3_column_int64(stmt, col));
    case SQLITE_FLOAT:
      return String(sqlite3_column_double(stmt, col), 6);
    case SQLITE_NULL:
      return "null";
    default:
      return "\"" + jsonEscape(colText(stmt, col)) + "\"";
  }
}

bool addColumnIfMissing(const char* tableName, const char* columnName, const char* alterSql) {
  if (tableHasColumn(tableName, columnName)) {
    Serial.print("Column already exists: ");
    Serial.print(tableName);
    Serial.print(".");
    Serial.println(columnName);
    return true;
  }

  char* errMsg = nullptr;
  int rc = sqlite3_exec(db, alterSql, nullptr, nullptr, &errMsg);

  if (rc != SQLITE_OK) {
    String err = errMsg ? String(errMsg) : "unknown";

    // Если колонка появилась раньше или уже была — считаем успешным
    if (err.indexOf("duplicate column name") >= 0) {
      Serial.print("Column duplicate ignored: ");
      Serial.println(err);
      if (errMsg) sqlite3_free(errMsg);
      return true;
    }

    Serial.print("SQL error ALTER: ");
    Serial.println(err);

    if (errMsg) sqlite3_free(errMsg);
    return false;
  }

  Serial.print("Column added: ");
  Serial.print(tableName);
  Serial.print(".");
  Serial.println(columnName);

  return true;
}
// ======================= Database =======================
bool initDatabase() {
  int rc = sqlite3_open(DB_SQLITE_PATH, &db);
  if (rc != SQLITE_OK) {
    Serial.print("Не удалось открыть SQLite базу: ");
    Serial.println(sqlite3_errmsg(db));
    return false;
  }

  execSQL("PRAGMA foreign_keys = ON;");

  bool ok = true;

  ok &= execSQL(
    "CREATE TABLE IF NOT EXISTS devices ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "properties TEXT,"
    "init_string TEXT,"
    "ip TEXT,"
    "port TEXT"
    ");"
  );
 
  ok &= execSQL(
    "CREATE TABLE IF NOT EXISTS functions ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "comment TEXT"
    ");"
  );

  ok &= execSQL(
    "CREATE TABLE IF NOT EXISTS receive_functions ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "device_id INTEGER NOT NULL,"
    "function_id INTEGER NOT NULL,"
    "get_code TEXT,"
    "comment TEXT,"
    "FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE CASCADE,"
    "FOREIGN KEY(function_id) REFERENCES functions(id) ON DELETE CASCADE"
    ");"
  );

  ok &= execSQL(
    "CREATE TABLE IF NOT EXISTS algorithms ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "algorithm_text TEXT,"
    "is_active INTEGER NOT NULL DEFAULT 0"
    ");"
  );

  ok &= execSQL(
    "CREATE TABLE IF NOT EXISTS logs ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "log_date TEXT NOT NULL,"
    "log_time TEXT NOT NULL,"
    "device_id INTEGER,"
    "value TEXT,"
    "direction TEXT CHECK(direction IN ('получение','установка')) NOT NULL,"
    "FOREIGN KEY(device_id) REFERENCES devices(id) ON DELETE SET NULL"
    ");"
  );

if (ok) {
  ok &= addColumnIfMissing(
    "algorithms",
    "is_active",
    "ALTER TABLE algorithms ADD COLUMN is_active INTEGER NOT NULL DEFAULT 0;"
  );
}

  return ok;
}
// ======================= Static files =======================
void handleRoot() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "index.html не найден. Загрузи папку data через pio run -t uploadfs");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}
void handleBD() {
  File file = LittleFS.open("/index.html", "r");
  if (!file) {
    server.send(404, "text/plain; charset=utf-8", "index.html не найден. Загрузи папку data через pio run -t uploadfs");
    return;
  }
  server.streamFile(file, "text/html");
  file.close();
}
void handleStaticFile(const char* path, const char* contentType) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    server.send(404, "text/plain", "File not found");
    return;
  }
  server.streamFile(file, contentType);
  file.close();
}

// ======================= Devices API =======================
void handleGetDevices() {
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "SELECT id, name, properties, init_string,ip,port FROM devices ORDER BY id DESC;")) return;

  String json = "[";
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
    json += ",\"properties\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
    json += ",\"ip\":\"" + jsonEscape(colText(stmt, 4)) + "\"";
    json += ",\"port\":\"" + jsonEscape(colText(stmt, 5)) + "\"";
    json += ",\"init_string\":\"" + jsonEscape(colText(stmt, 3)) + "\"}";
  }
  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleSaveDevice() {
  DynamicJsonDocument doc(4096);
  if (!parseJsonBody(doc)) return;

  int id = doc["id"] | 0;
  String name = doc["name"] | "";
  String properties = doc["properties"] | "";
  String initString = doc["init_string"] | "";
  String initip = doc["ip"] | "";
   String initport = doc["port"] | "";
  name.trim();
  if (name.length() == 0) { sendError(400, "Название прибора пустое"); return; }

  sqlite3_stmt* stmt = nullptr;
  if (id > 0) {
    if (!dbPrepare(&stmt, "UPDATE devices SET name=?, properties=?, init_string=?,ip=?,port=? WHERE id=?;")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, properties);
    bindText(stmt, 3, initString);
    bindText(stmt, 4, initip); 
    bindText(stmt, 5, initport);
    sqlite3_bind_int(stmt, 6, id);
  } else {
    if (!dbPrepare(&stmt, "INSERT INTO devices(name, properties, init_string,ip,port) VALUES(?,?,?,?,?);")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, properties);
    bindText(stmt, 3, initString);
    bindText(stmt, 4, initip);
        bindText(stmt, 5, initport);
  }

  if (dbStepDone(stmt)) sendOk();
}

void handleDeleteDevice() {
  int id = getIdArg();
  if (id <= 0) { sendError(400, "Не передан id"); return; }
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "DELETE FROM devices WHERE id=?;")) return;
  sqlite3_bind_int(stmt, 1, id);
  if (dbStepDone(stmt)) sendOk();
}

// ======================= Functions API =======================
void handleGetFunctions() {
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "SELECT id, name, comment FROM functions ORDER BY id DESC;")) return;

  String json = "[";
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
    json += ",\"comment\":\"" + jsonEscape(colText(stmt, 2)) + "\"}";
  }
  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleSaveFunction() {
  DynamicJsonDocument doc(2048);
  if (!parseJsonBody(doc)) return;

  int id = doc["id"] | 0;
  String name = doc["name"] | "";
  String comment = doc["comment"] | "";
  name.trim();
  if (name.length() == 0) { sendError(400, "Название функции пустое"); return; }

  sqlite3_stmt* stmt = nullptr;
  if (id > 0) {
    if (!dbPrepare(&stmt, "UPDATE functions SET name=?, comment=? WHERE id=?;")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, comment);
    sqlite3_bind_int(stmt, 3, id);
  } else {
    if (!dbPrepare(&stmt, "INSERT INTO functions(name, comment) VALUES(?,?);")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, comment);
  }

  if (dbStepDone(stmt)) sendOk();
}

void handleDeleteFunction() {
  int id = getIdArg();
  if (id <= 0) { sendError(400, "Не передан id"); return; }
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "DELETE FROM functions WHERE id=?;")) return;
  sqlite3_bind_int(stmt, 1, id);
  if (dbStepDone(stmt)) sendOk();
}

// ======================= Receive functions API =======================
void handleGetReceiveFunctions() {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
    "SELECT rf.id, rf.device_id, COALESCE(d.name,''), rf.function_id, COALESCE(f.name,''), rf.get_code, rf.comment "
    "FROM receive_functions rf "
    "LEFT JOIN devices d ON d.id = rf.device_id "
    "LEFT JOIN functions f ON f.id = rf.function_id "
    "ORDER BY rf.id DESC;";

  if (!dbPrepare(&stmt, sql)) return;

  String json = "[";
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"device_id\":" + String(sqlite3_column_int(stmt, 1));
    json += ",\"device_name\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
    json += ",\"function_id\":" + String(sqlite3_column_int(stmt, 3));
    json += ",\"function_name\":\"" + jsonEscape(colText(stmt, 4)) + "\"";
    json += ",\"get_code\":\"" + jsonEscape(colText(stmt, 5)) + "\"";
    json += ",\"comment\":\"" + jsonEscape(colText(stmt, 6)) + "\"}";
  }
  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleSaveReceiveFunction() {
  DynamicJsonDocument doc(8192);
  if (!parseJsonBody(doc)) return;

  int id = doc["id"] | 0;
  int deviceId = doc["device_id"] | 0;
  int functionId = doc["function_id"] | 0;
  String getCode = doc["get_code"] | "";
  String comment = doc["comment"] | "";

  if (deviceId <= 0) { sendError(400, "Не выбран прибор"); return; }
  if (functionId <= 0) { sendError(400, "Не выбрана функция"); return; }

  sqlite3_stmt* stmt = nullptr;
  if (id > 0) {
    if (!dbPrepare(&stmt, "UPDATE receive_functions SET device_id=?, function_id=?, get_code=?, comment=? WHERE id=?;")) return;
    sqlite3_bind_int(stmt, 1, deviceId);
    sqlite3_bind_int(stmt, 2, functionId);
    bindText(stmt, 3, getCode);
    bindText(stmt, 4, comment);
    sqlite3_bind_int(stmt, 5, id);
  } else {
    if (!dbPrepare(&stmt, "INSERT INTO receive_functions(device_id, function_id, get_code, comment) VALUES(?,?,?,?);")) return;
    sqlite3_bind_int(stmt, 1, deviceId);
    sqlite3_bind_int(stmt, 2, functionId);
    bindText(stmt, 3, getCode);
    bindText(stmt, 4, comment);
  }

  if (dbStepDone(stmt)) sendOk();
}

void handleDeleteReceiveFunction() {
  int id = getIdArg();
  if (id <= 0) { sendError(400, "Не передан id"); return; }
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "DELETE FROM receive_functions WHERE id=?;")) return;
  sqlite3_bind_int(stmt, 1, id);
  if (dbStepDone(stmt)) sendOk();
}

// ======================= Algorithms API =======================
void handleGetAlgorithms() {
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "SELECT id, name, algorithm_text, is_active FROM algorithms ORDER BY id DESC;")) return;

  String json = "[";
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"name\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
    json += ",\"algorithm_text\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
    json += ",\"is_active\":" + String(sqlite3_column_int(stmt, 3));
    json += "}";
  }
  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleSaveAlgorithm() {
  DynamicJsonDocument doc(8192);
  if (!parseJsonBody(doc)) return;

  int id = doc["id"] | 0;
  String name = doc["name"] | "";
  String algorithmText = doc["algorithm_text"] | "";
  int isActive = doc["is_active"] | 0;
  name.trim();
  if (name.length() == 0) { sendError(400, "Название алгоритма пустое"); return; }

  sqlite3_stmt* stmt = nullptr;
  if (id > 0) {
    if (!dbPrepare(&stmt, "UPDATE algorithms SET name=?, algorithm_text=?, is_active=? WHERE id=?;")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, algorithmText);
    sqlite3_bind_int(stmt, 3, isActive ? 1 : 0);
    sqlite3_bind_int(stmt, 4, id);
  } else {
    if (!dbPrepare(&stmt, "INSERT INTO algorithms(name, algorithm_text, is_active) VALUES(?,?,?);")) return;
    bindText(stmt, 1, name);
    bindText(stmt, 2, algorithmText);
    sqlite3_bind_int(stmt, 3, isActive ? 1 : 0);
  }

  if (dbStepDone(stmt)) sendOk();
}
String nowDateString() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 50)) {
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
    return String(buf);
  }
  return String(millis());
}
void addDebugLog(int deviceId, const String& code, const String& source) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "INSERT INTO logs(log_date, log_time, device_id, value, direction) VALUES(?,?,?,?,?);", -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.print("Debug log SQL error: ");
    Serial.println(sqlite3_errmsg(db));
    return;
  }

  String date = nowDateString();
  String time = nowTimeString();
  String value = "DEBUG [" + source + "]: " + code;

  bindText(stmt, 1, date);
  bindText(stmt, 2, time);

  if (deviceId > 0) sqlite3_bind_int(stmt, 3, deviceId);
  else sqlite3_bind_null(stmt, 3);

  bindText(stmt, 4, value);
  bindText(stmt, 5, "установка");

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    Serial.print("Debug log insert error: ");
    Serial.println(sqlite3_errmsg(db));
  }

  sqlite3_finalize(stmt);
}
void handleDeleteAlgorithm() {
  int id = getIdArg();
  if (id <= 0) { sendError(400, "Не передан id"); return; }
  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "DELETE FROM algorithms WHERE id=?;")) return;
  sqlite3_bind_int(stmt, 1, id);
  if (dbStepDone(stmt)) sendOk();
}
void handleRunAlgorithm() {
  int id = getIdArg();

  if (id <= 0) {
    sendError(400, "Не передан id алгоритма");
    return;
  }

  sqlite3_stmt* stmt = nullptr;

  if (!dbPrepare(&stmt, "SELECT name, algorithm_text FROM algorithms WHERE id=?;")) {
    return;
  }

  sqlite3_bind_int(stmt, 1, id);

  int rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    sendError(404, "Алгоритм не найден");
    return;
  }

  String name = colText(stmt, 0);
  String code = colText(stmt, 1);

  sqlite3_finalize(stmt);

  code.trim();

  if (code.length() == 0) {
    sendError(400, "Текст алгоритма пустой");
    return;
  }

  bool luaOk = false;
  String output = runLuaAlgorithm(code, luaOk);

  addDebugLog(0, "LUA algorithm #" + String(id) + " " + name + "\n" + output, "algorithm");

  String json = "{\"ok\":";
  json += luaOk ? "true" : "false";
  json += ",\"source\":\"algorithm\"";
  json += ",\"algorithm_id\":" + String(id);
  json += ",\"name\":\"" + jsonEscape(name) + "\"";
  json += ",\"time\":\"" + jsonEscape(nowTimeString()) + "\"";
  json += ",\"output\":\"" + jsonEscape(output) + "\"";
  json += ",\"code\":\"" + jsonEscape(code) + "\"";
  json += "}";

  sendJson(luaOk ? 200 : 500, json);
}

void runActiveAlgorithmsIfNeeded() {
  if (digitalRead(USER_debug_pin) != LOW) {
    lastActiveAlgorithmRunMs = millis();
    return;
  }

  unsigned long nowMs = millis();
  if (nowMs - lastActiveAlgorithmRunMs < ACTIVE_ALGORITHM_INTERVAL_MS) return;
  lastActiveAlgorithmRunMs = nowMs;

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT id, name, algorithm_text FROM algorithms WHERE is_active=1 ORDER BY id ASC;", -1, &stmt, nullptr) != SQLITE_OK) {
    Serial.print("Active algorithm SQL error: ");
    Serial.println(sqlite3_errmsg(db));
    return;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    int id = sqlite3_column_int(stmt, 0);
    String name = colText(stmt, 1);
    String code = colText(stmt, 2);
    code.trim();
    if (code.length() == 0) continue;

    bool luaOk = false;
    String output = runLuaAlgorithm(code, luaOk);
    addDebugLog(0, "AUTO LUA #" + String(id) + " " + name + "\n" + output, luaOk ? "algorithm:auto" : "algorithm:auto:error");
    yield();
  }

  sqlite3_finalize(stmt);
}
// ======================= Logs API =======================
void handleGetLogs() {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
    "SELECT l.id, l.log_date, l.log_time, l.device_id, COALESCE(d.name,''), l.value, l.direction "
    "FROM logs l "
    "LEFT JOIN devices d ON d.id = l.device_id "
    "ORDER BY l.id DESC LIMIT 300;";
Serial.println(sql);
Serial.println("ищем 2");
  if (!dbPrepare(&stmt, sql)) return;

  String json = "[";
  bool first = true;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (!first) json += ",";
    first = false;
    json += "{\"id\":" + String(sqlite3_column_int(stmt, 0));
    json += ",\"log_date\":\"" + jsonEscape(colText(stmt, 1)) + "\"";
    json += ",\"log_time\":\"" + jsonEscape(colText(stmt, 2)) + "\"";
    json += ",\"device_id\":" + String(sqlite3_column_int(stmt, 3));
    json += ",\"device_name\":\"" + jsonEscape(colText(stmt, 4)) + "\"";
    json += ",\"value\":\"" + jsonEscape(colText(stmt, 5)) + "\"";
    json += ",\"direction\":\"" + jsonEscape(colText(stmt, 6)) + "\"}";
  }
  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleAddLog() {
  DynamicJsonDocument doc(2048);
  if (!parseJsonBody(doc)) return;

  int deviceId = doc["device_id"] | 0;
  String value = doc["value"] | "";
  String direction = doc["direction"] | "получение";
  String logDate = doc["log_date"] | "";
  String logTime = doc["log_time"] | "";

  if (direction != "получение" && direction != "установка") {
    sendError(400, "Направление должно быть: получение или установка");
    return;
  }

  sqlite3_stmt* stmt = nullptr;
  if (!dbPrepare(&stmt, "INSERT INTO logs(log_date, log_time, device_id, value, direction) VALUES(?,?,?,?,?);")) return;
  bindText(stmt, 1, logDate);
  bindText(stmt, 2, logTime);
  if (deviceId > 0) sqlite3_bind_int(stmt, 3, deviceId);
  else sqlite3_bind_null(stmt, 3);
  bindText(stmt, 4, value);
  bindText(stmt, 5, direction);

  if (dbStepDone(stmt)) sendOk();
}

void handleClearLogs() {
  if (!execSQL("DELETE FROM logs;")) {
    sendError(500, "Ошибка очистки log");
    return;
  }
  sendOk();
}

// ======================= Debug API =======================





// ======================= Lua API =======================

static String luaOutput;

static int luaPrint(lua_State* L) {
  int n = lua_gettop(L);

  for (int i = 1; i <= n; i++) {
    const char* s = lua_tostring(L, i);
    if (s) luaOutput += s;
    else luaOutput += "[non-string]";

    if (i < n) luaOutput += "\t";
  }

  luaOutput += "\n";
  return 0;
}

static int luaMillis(lua_State* L) {
  lua_pushinteger(L, millis());
  return 1;
}

static int luaDelay(lua_State* L) {
  int ms = luaL_checkinteger(L, 1);
  if (ms < 0) ms = 0;
  if (ms > 10000) ms = 10000; // защита от слишком долгой задержки
  delay(ms);
  return 0;
}

static int luaLog(lua_State* L) {
  const char* value = luaL_checkstring(L, 1);
  int deviceId = 0;
  const char* direction = "получение";

  if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
    deviceId = lua_tointeger(L, 2);
  }

  if (lua_gettop(L) >= 3 && lua_isstring(L, 3)) {
    direction = lua_tostring(L, 3);
  }

  addDebugLog(deviceId, String(value), String("lua:") + direction);
  return 0;
}

static int luaSendSMB(lua_State* L) {
  const char* cmd = luaL_checkstring(L, 1);
  String answer = send_read(&Generator, String(cmd));
  lua_pushstring(L, answer.c_str());
  return 1;
}

static int luaSend_MX_2SP4N(lua_State* L) {
  const char* cmd = luaL_checkstring(L, 1);
  String answer = send_read(&MX_4, String(cmd));
  lua_pushstring(L, answer.c_str());
  return 1;
}

static int luaSendFSW(lua_State* L) {
  const char* cmd = luaL_checkstring(L, 1);
  String answer = send_read(&Spectr_Analyzer, String(cmd));
  lua_pushstring(L, answer.c_str());
  return 1;
}

static int luaSend(lua_State* L) {
  const char* dev = luaL_checkstring(L, 1);
  const char* cmd = luaL_checkstring(L, 2);

  String device = String(dev);
  device.toLowerCase();

  String answer;

  if (device == "smb" || device == "generator" || device == "генератор") {
    answer = send_read(&Generator_get, String(cmd));
  } else if (device == "fsw" || device == "spectr" || device == "analyzer" || device == "анализатор") {
    answer = send_read(&Spectr_Analyzer, String(cmd));
  } else {
    answer = "ERROR: unknown device: " + device;
  }

  lua_pushstring(L, answer.c_str());
  return 1;
}

void tracec(void *data,const char *sql){
Serial.println((const char*)sql);

}





String  lua_get_String(lua_State* L, int index )
{
  String str = "";

  int type = lua_type(L, index);  

  if (type  == LUA_TNUMBER)
  {
    if(lua_isinteger(L, index))
    {  
      str = String(luaL_checkinteger(L, index));
    }
    else
    {      
      str = String(luaL_checknumber(L, index), decimal_plase);
    }           
  }
  if (type  == LUA_TSTRING)
  {
    str = String(luaL_checkstring(L, index));
  }
  return str;
}



static int lua_send_val(lua_State* L) 
{
  const char* dev   = luaL_checkstring(L, 1);
  const char* func  = luaL_checkstring(L, 2);

  //float nval = 0;
  String nval = "";   
  //String unit = "";  
  const int col = lua_gettop(L);
 
  if (col == 3)
  {
    nval = lua_get_String(L,3); //value
  }
  if (col == 4)
  {
    nval = lua_get_String(L,3); //value
    nval =  nval + " " + lua_get_String(L,4); //unit
  }

  sqlite3_stmt* stmt = nullptr;
  const char* sql =
  "SELECT rf.id, rf.device_id, COALESCE(d.name,''), rf.function_id, COALESCE(f.name,''), rf.get_code, rf.comment, d.ip, d.port "
  "FROM receive_functions rf "
  "LEFT JOIN devices d ON d.id = rf.device_id "
  "LEFT JOIN functions f ON f.id = rf.function_id "
  "WHERE d.name=? and f.name=?;"; 

  if (!dbPrepare(&stmt, sql)) 
  {
    return 0;
  } 
  sqlite3_bind_text(stmt, 1, dev, strlen(dev), SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, func, strlen(func), SQLITE_STATIC);

  //Serial.println(sql);

  int rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) 
  {
    sqlite3_finalize(stmt);
    sendError(404, "Алгоритм не найден");
    return 0;
  } 
    
  String code = colText(stmt, 5);
  String ip   = colText(stmt, 7);
  String port_str = colText(stmt, 8);
  uint16_t port = (uint16_t)port_str.toInt();  

  if (!(col<3))  
  {
    code = code + " " + nval;//формирование строки управления прибором 
  }   

  display.print(ip);
  display.print(":");
  display.println(port);
  display.println(code);
  display.println(nval);

  // if (unit!=nullptr)
  // {
  //   display.println(unit);
  // }
  Device.IP_add.fromString(ip.c_str()); 
  Device.Port = port;   
  String val  = send_read(&Device, code);//запуск команды управлени

  sqlite3_finalize(stmt);
  lua_pushstring(L, val.c_str());
  return 1;
}

static int lua_send_on_off(lua_State* L) 
{
  const char* dev   = luaL_checkstring(L, 1);
  const char* func  = luaL_checkstring(L, 2);
  float nval = 0;   const char* unit;

  const int col=lua_gettop(L);
  if (col>2)
  { 
    nval  = luaL_checknumber(L, 3);
    unit  = luaL_checkstring(L, 4);
  }
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
  "SELECT rf.id, rf.device_id, COALESCE(d.name,''), rf.function_id, COALESCE(f.name,''), rf.get_code, rf.comment, d.ip, d.port "
  "FROM receive_functions rf "
  "LEFT JOIN devices d ON d.id = rf.device_id "
  "LEFT JOIN functions f ON f.id = rf.function_id "
  "WHERE d.name=? and f.name=?;"; 

  if (!dbPrepare(&stmt, sql)) 
  {
    return 0;
  } 
  sqlite3_bind_text(stmt, 1, dev, strlen(dev), SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, func, strlen(func), SQLITE_STATIC);

  Serial.println(sql);
  int rc = sqlite3_step(stmt);

  if (rc != SQLITE_ROW) 
  {
    sqlite3_finalize(stmt);
    sendError(404, "Алгоритм не найден");
    return 0;
  } 
    
  String code = colText(stmt, 5);
  String ip   = colText(stmt, 7);
  String port_str = colText(stmt, 8);
  uint16_t port = (uint16_t)port_str.toInt();  

  if (!(col<3))  
  {
    code = code + " " + String(nval, decimal_plase) + unit;//формирование строки управления прибором 
  }   

  display.print(ip);
  display.print(":");
  display.println(port);
  display.println(code);
  display.println(nval);

  if (unit!=nullptr)
  {
    display.println(unit);
  } 
  Device.IP_add.fromString(ip.c_str()); 
  Device.Port = port;   
  String val  = send_read(&Device, code);//запуск команды управлени

  //String val  = send_read(&Generator,code);//запуск команды управления 
  //String val = send_read(&Spectr_Analyzer,code);//запуск команды управления 

  sqlite3_finalize(stmt);
  lua_pushstring(L, val.c_str());
  return 1;
}


String runLuaAlgorithm(const String& code, bool& ok) 
{
  ok = false;
  luaOutput = "";

  lua_State* L = luaL_newstate();
  if (!L) {
    return "Lua error: cannot create lua state";
  }

  luaL_openlibs(L);
  lua_register(L, "print", luaPrint);
  lua_register(L, "millis", luaMillis);
  lua_register(L, "delay", luaDelay);
  lua_register(L, "log", luaLog);
  lua_register(L, "send_smb", luaSendSMB);
  lua_register(L, "send_fsw", luaSendFSW);
  lua_register(L, "send", luaSend);
  lua_register(L, "send_val", lua_send_val);
  
  lua_register(L, "Delay_ms", lua_delay_ms);

  lua_register(L, "USER_mode", lua_USER_mode);  

  lua_register(L, "NGK_0dB" , lua_NGK_0dB);
  lua_register(L, "NGK_30dB", lua_NGK_30dB);
  lua_register(L, "UZG_0dB" , lua_UZG_0dB);
  lua_register(L, "UZG_15dB", lua_UZG_15dB);

  int rc = luaL_loadstring(L, code.c_str());
  if (rc != LUA_OK) {
    String err = lua_tostring(L, -1);
    lua_close(L);
    return "Lua load error: " + err;
  }

  rc = lua_pcall(L, 0, LUA_MULTRET, 0);
  if (rc != LUA_OK) {
    String err = lua_tostring(L, -1);
    lua_close(L);
    return "Lua runtime error: " + err;
  }

  int results = lua_gettop(L);
  if (results > 0) {
    luaOutput += "\nreturn:\n";
    for (int i = 1; i <= results; i++) {
      if (lua_isstring(L, i) || lua_isnumber(L, i)) {
        luaOutput += lua_tostring(L, i);
      } else if (lua_isboolean(L, i)) {
        luaOutput += lua_toboolean(L, i) ? "true" : "false";
      } else if (lua_isnil(L, i)) {
        luaOutput += "nil";
      } else {
        luaOutput += "[non-printable]";
      }
      luaOutput += "\n";
    }
  }

  lua_close(L);

  ok = true;
  return luaOutput;
}

void handleDebugStart() {
  DynamicJsonDocument doc(8192);
  //DynamicJsonDocument doc(1024);
  if (!parseJsonBody(doc)) return;

  String code = doc["code"] | "";
  String source = doc["source"] | "manual";
  int receiveId = doc["receive_id"] | 0;
  int deviceId = doc["device_id"] | 0;
  int functionId = doc["function_id"] | 0;

  code.trim();

  if (code.length() == 0) {
    sendError(400, "Пустой код debug-команды");
    return;
  } 
  String str;
  //str = send_read(Generator, code);//посылка запроса прибору
  str =       send_IDN(&Generator);//посылка запроса прибору
  str = str + send_IDN(&Generator_get);//посылка запроса прибору
  str = str + send_IDN(&Spectr_Analyzer);//посылка запроса прибору
  str = str + send_IDN(&DC_source);//посылка запроса прибору
  str = str + send_IDN_MX(&MX_6);//посылка запроса прибору
  //str = send_read(Spectr_Analyzer, code);//посылка запроса прибору

  // Serial.println(" TEST " + Generator.manufacture);
  // Serial.println(" TEST " + Generator.instrument);
  // Serial.println(" TEST " + Generator.ser_num);
  // Serial.println(" TEST " + Generator.soft_ver);

  // Serial.println(" TEST " + MX_6.manufacture);
  // Serial.println(" TEST " + MX_6.instrument);
  // Serial.println(" TEST " + MX_6.ser_num);
  // Serial.println(" TEST " + MX_6.soft_ver);

  // ВАЖНО:
  // ESP32 не может выполнить C++/Arduino-код, присланный строкой, как скрипт.
  // Этот handler принимает код/команду, пишет в Serial и в log.
  // Ниже потом можно добавить свой парсер команд: digitalWrite, read, init и т.д.

/*
  Serial.println("========== DEBUG START ==========");
  Serial.print("source: ");
  Serial.println(source);
  Serial.print("receive_id: ");
  Serial.println(receiveId);
  Serial.print("device_id: ");
  Serial.println(deviceId);
  Serial.print("function_id: ");
  Serial.println(functionId);
  Serial.println("code:");
*/
  //Serial.println(str);
  //Serial.println("=================================");

  addDebugLog(deviceId, code, source);

  String json = "{\"ok\":true";
  json += ",\"source\":\"" + jsonEscape(source) + "\"";
  json += ",\"receive_id\":" + String(receiveId);
  json += ",\"device_id\":" + String(deviceId);
  json += ",\"function_id\":" + String(functionId);
  json += ",\"time\":\"" + jsonEscape(nowTimeString()) + "\"";
  json += ",\"code\":\"" + jsonEscape(str) + "\"";
  json += "}";

  sendJson(200, json);
}


// ======================= Database import/export API =======================
void closeDatabase() {
  if (db) {
    sqlite3_close(db);
    db = nullptr;
  }
}

bool reopenDatabase() {
  closeDatabase();
  return initDatabase();
}

bool copyLittleFsFile(const char* from, const char* to) {
  File src = LittleFS.open(from, "r");
  if (!src) return false;

  if (LittleFS.exists(to)) LittleFS.remove(to);

  File dst = LittleFS.open(to, "w");
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t buf[1024];
  while (src.available()) {
    size_t n = src.read(buf, sizeof(buf));
    if (n > 0) {
      size_t w = dst.write(buf, n);
      if (w != n) {
        src.close();
        dst.close();
        return false;
      }
    }
    delay(0);
  }

  src.close();
  dst.close();
  return true;
}

bool validateImportedDatabase(const char* sqlitePath) {
  sqlite3* testDb = nullptr;
  int rc = sqlite3_open(sqlitePath, &testDb);
  if (rc != SQLITE_OK) {
    if (testDb) sqlite3_close(testDb);
    return false;
  }

  const char* sql =
    "SELECT name FROM sqlite_master "
    "WHERE type='table' AND name IN ('devices','functions','receive_functions','algorithms','logs');";

  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(testDb, sql, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    sqlite3_close(testDb);
    return false;
  }

  int count = 0;
  while (sqlite3_step(stmt) == SQLITE_ROW) count++;

  sqlite3_finalize(stmt);
  sqlite3_close(testDb);

  return count >= 4;
}

void handleDbInfo() {
  File dbFile = LittleFS.open(DB_PATH, "r");
  size_t dbSize = dbFile ? dbFile.size() : 0;
  if (dbFile) dbFile.close();

  File backupFile = LittleFS.open(DB_BACKUP_PATH, "r");
  size_t backupSize = backupFile ? backupFile.size() : 0;
  if (backupFile) backupFile.close();

  String json = "{\"ok\":true";
  json += ",\"db_exists\":" + String(LittleFS.exists(DB_PATH) ? "true" : "false");
  json += ",\"db_size\":" + String(dbSize);
  json += ",\"backup_exists\":" + String(LittleFS.exists(DB_BACKUP_PATH) ? "true" : "false");
  json += ",\"backup_size\":" + String(backupSize);
  json += ",\"total_bytes\":" + String(LittleFS.totalBytes());
  json += ",\"used_bytes\":" + String(LittleFS.usedBytes());
  json += "}";
  sendJson(200, json);
}

void handleDbTables() {
  sqlite3_stmt* stmt = nullptr;
  const char* sql =
    "SELECT name FROM sqlite_master "
    "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
    "ORDER BY name ASC;";

  if (!dbPrepare(&stmt, sql)) return;

  String json = "[";
  bool firstTable = true;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    String tableName = colText(stmt, 0);
    if (!firstTable) json += ",";
    firstTable = false;

    String countSql = "SELECT COUNT(*) FROM " + tableName + ";";
    sqlite3_stmt* countStmt = nullptr;
    sqlite3_int64 rowCount = 0;
    if (sqlite3_prepare_v2(db, countSql.c_str(), -1, &countStmt, nullptr) == SQLITE_OK) {
      if (sqlite3_step(countStmt) == SQLITE_ROW) {
        rowCount = sqlite3_column_int64(countStmt, 0);
      }
    }
    if (countStmt) sqlite3_finalize(countStmt);

    json += "{\"name\":\"" + jsonEscape(tableName) + "\"";
    json += ",\"row_count\":" + int64ToString(rowCount);
    json += "}";
  }

  sqlite3_finalize(stmt);
  json += "]";
  sendJson(200, json);
}

void handleDbTableData() {
  String tableName = server.arg("name");
  int limit = server.hasArg("limit") ? server.arg("limit").toInt() : 100;
  if (limit <= 0) limit = 100;
  if (limit > 300) limit = 300;

  if (!dbTableExists(tableName)) {
    sendError(400, "Некорректное имя таблицы");
    return;
  }

  String json = "{\"ok\":true";
  json += ",\"table\":\"" + jsonEscape(tableName) + "\"";

  sqlite3_stmt* colStmt = nullptr;
  String pragmaSql = "PRAGMA table_info(" + tableName + ");";
  if (sqlite3_prepare_v2(db, pragmaSql.c_str(), -1, &colStmt, nullptr) != SQLITE_OK) {
    sendError(500, sqlite3_errmsg(db));
    return;
  }

  json += ",\"columns\":[";
  bool firstCol = true;
  while (sqlite3_step(colStmt) == SQLITE_ROW) {
    if (!firstCol) json += ",";
    firstCol = false;
    json += "{";
    json += "\"cid\":" + String(sqlite3_column_int(colStmt, 0));
    json += ",\"name\":\"" + jsonEscape(colText(colStmt, 1)) + "\"";
    json += ",\"type\":\"" + jsonEscape(colText(colStmt, 2)) + "\"";
    json += ",\"notnull\":" + String(sqlite3_column_int(colStmt, 3));
    json += ",\"default_value\":\"" + jsonEscape(colText(colStmt, 4)) + "\"";
    json += ",\"pk\":" + String(sqlite3_column_int(colStmt, 5));
    json += "}";
  }
  sqlite3_finalize(colStmt);
  json += "]";

  String dataSql = "SELECT * FROM " + tableName + " ORDER BY rowid DESC LIMIT " + String(limit) + ";";
  sqlite3_stmt* dataStmt = nullptr;
  if (sqlite3_prepare_v2(db, dataSql.c_str(), -1, &dataStmt, nullptr) != SQLITE_OK) {
    sendError(500, sqlite3_errmsg(db));
    return;
  }

  json += ",\"rows\":[";
  bool firstRow = true;
  int colCount = sqlite3_column_count(dataStmt);
  while (sqlite3_step(dataStmt) == SQLITE_ROW) {
    if (!firstRow) json += ",";
    firstRow = false;
    json += "{";
    for (int i = 0; i < colCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + jsonEscape(String(sqlite3_column_name(dataStmt, i))) + "\":";
      json += sqliteValueToJson(dataStmt, i);
    }
    json += "}";
  }
  sqlite3_finalize(dataStmt);
  json += "]";
  json += "}";
  sendJson(200, json);
}

void handleDbQuery() {
  DynamicJsonDocument doc(16384);
  if (!parseJsonBody(doc)) return;

  String sql = doc["sql"] | "";
  sql.trim();
  if (sql.length() == 0) {
    sendError(400, "Пустой SQL-запрос");
    return;
  }

  sqlite3_stmt* stmt = nullptr;
  const char* tail = nullptr;
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, &tail);
  if (rc != SQLITE_OK || !stmt) {
    if (stmt) sqlite3_finalize(stmt);
    sendError(400, sqlite3_errmsg(db));
    return;
  }

  if (!isWhitespaceOnly(tail)) {
    sqlite3_finalize(stmt);
    sendError(400, "Разрешён только один SQL-запрос за раз");
    return;
  }

  int colCount = sqlite3_column_count(stmt);
  String json = "{\"ok\":true";
  json += ",\"sql\":\"" + jsonEscape(sql) + "\"";

  if (colCount > 0) {
    json += ",\"type\":\"resultset\"";
    json += ",\"columns\":[";
    for (int i = 0; i < colCount; i++) {
      if (i > 0) json += ",";
      json += "\"" + jsonEscape(String(sqlite3_column_name(stmt, i))) + "\"";
    }
    json += "]";

    json += ",\"rows\":[";
    bool firstRow = true;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      if (!firstRow) json += ",";
      firstRow = false;
      json += "{";
      for (int i = 0; i < colCount; i++) {
        if (i > 0) json += ",";
        json += "\"" + jsonEscape(String(sqlite3_column_name(stmt, i))) + "\":";
        json += sqliteValueToJson(stmt, i);
      }
      json += "}";
    }
    json += "]";

    if (rc != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      sendError(400, sqlite3_errmsg(db));
      return;
    }
  } else {
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      sqlite3_finalize(stmt);
      sendError(400, sqlite3_errmsg(db));
      return;
    }
    json += ",\"type\":\"mutation\"";
    json += ",\"affected_rows\":" + String(sqlite3_changes(db));
  }

  sqlite3_finalize(stmt);
  json += "}";
  sendJson(200, json);
}

void handleDbTableSave() {
  DynamicJsonDocument doc(16384);
  if (!parseJsonBody(doc)) return;

  String tableName = doc["table"] | "";
  if (!dbTableExists(tableName)) {
    sendError(400, "Некорректное имя таблицы");
    return;
  }

  JsonObject row = doc["row"].as<JsonObject>();
  if (row.isNull()) {
    sendError(400, "Нет данных строки");
    return;
  }

  sqlite3_stmt* colStmt = nullptr;
  String pragmaSql = "PRAGMA table_info(" + tableName + ");";
  if (sqlite3_prepare_v2(db, pragmaSql.c_str(), -1, &colStmt, nullptr) != SQLITE_OK) {
    sendError(500, sqlite3_errmsg(db));
    return;
  }

  String pkName = "";
  String columns[32];
  int columnCount = 0;
  while (sqlite3_step(colStmt) == SQLITE_ROW && columnCount < 32) {
    String colName = colText(colStmt, 1);
    columns[columnCount++] = colName;
    if (sqlite3_column_int(colStmt, 5) == 1) pkName = colName;
  }
  sqlite3_finalize(colStmt);

  if (pkName.length() == 0) {
    sendError(400, "Таблица без одиночного primary key не поддерживается");
    return;
  }

  bool hasPk = row.containsKey(pkName);
  String pkValue = hasPk ? String((const char*)row[pkName].as<const char*>()) : "";
  if (hasPk && pkValue == "null") hasPk = false;

  String sql;
  sqlite3_stmt* stmt = nullptr;

  if (hasPk && pkValue.length() > 0) {
    sql = "UPDATE " + tableName + " SET ";
    bool first = true;
    int bindIndex = 1;
    for (int i = 0; i < columnCount; i++) {
      String colName = columns[i];
      if (colName == pkName || !row.containsKey(colName)) continue;
      if (!first) sql += ",";
      first = false;
      sql += colName + "=?";
    }
    if (first) {
      sendError(400, "Нет полей для обновления");
      return;
    }
    sql += " WHERE " + pkName + "=?";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      sendError(500, sqlite3_errmsg(db));
      return;
    }

    for (int i = 0; i < columnCount; i++) {
      String colName = columns[i];
      if (colName == pkName || !row.containsKey(colName)) continue;
      JsonVariant value = row[colName];
      if (value.isNull()) sqlite3_bind_null(stmt, bindIndex++);
      else bindText(stmt, bindIndex++, String((const char*)value.as<const char*>()));
    }
    bindText(stmt, bindIndex, pkValue);
  } else {
    String names = "";
    String values = "";
    int bindIndex = 1;
    for (int i = 0; i < columnCount; i++) {
      String colName = columns[i];
      if (!row.containsKey(colName)) continue;
      JsonVariant value = row[colName];
      if (colName == pkName) {
        String rawPk = value.isNull() ? "" : String((const char*)value.as<const char*>());
        if (rawPk.length() == 0) continue;
      }
      if (names.length() > 0) {
        names += ",";
        values += ",";
      }
      names += colName;
      values += "?";
    }
    if (names.length() == 0) {
      sendError(400, "Нет полей для вставки");
      return;
    }
    sql = "INSERT INTO " + tableName + "(" + names + ") VALUES(" + values + ")";

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
      sendError(500, sqlite3_errmsg(db));
      return;
    }

    for (int i = 0; i < columnCount; i++) {
      String colName = columns[i];
      if (!row.containsKey(colName)) continue;
      JsonVariant value = row[colName];
      if (colName == pkName) {
        String rawPk = value.isNull() ? "" : String((const char*)value.as<const char*>());
        if (rawPk.length() == 0) continue;
      }
      if (value.isNull()) sqlite3_bind_null(stmt, bindIndex++);
      else bindText(stmt, bindIndex++, String((const char*)value.as<const char*>()));
    }
  }

  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    String err = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sendError(400, err);
    return;
  }
  sqlite3_finalize(stmt);

  String json = "{\"ok\":true";
  json += ",\"table\":\"" + jsonEscape(tableName) + "\"";
  json += ",\"affected_rows\":" + String(sqlite3_changes(db));
  json += ",\"last_insert_id\":" + int64ToString(sqlite3_last_insert_rowid(db));
  json += "}";
  sendJson(200, json);
}

void handleDbTableDelete() {
  DynamicJsonDocument doc(4096);
  if (!parseJsonBody(doc)) return;

  String tableName = doc["table"] | "";
  if (!dbTableExists(tableName)) {
    sendError(400, "Некорректное имя таблицы");
    return;
  }

  String pkName = doc["pk_name"] | "";
  String pkValue = doc["pk_value"] | "";
  if (!isSafeSqlIdentifier(pkName) || pkValue.length() == 0) {
    sendError(400, "Некорректный primary key");
    return;
  }

  String sql = "DELETE FROM " + tableName + " WHERE " + pkName + "=?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    sendError(500, sqlite3_errmsg(db));
    return;
  }

  bindText(stmt, 1, pkValue);
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE) {
    String err = sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    sendError(400, err);
    return;
  }
  sqlite3_finalize(stmt);

  String json = "{\"ok\":true";
  json += ",\"table\":\"" + jsonEscape(tableName) + "\"";
  json += ",\"affected_rows\":" + String(sqlite3_changes(db));
  json += "}";
  sendJson(200, json);
}

void handleDbExport() {
  if (!LittleFS.exists(DB_PATH)) {
    sendError(404, "Файл базы /app.db не найден");
    return;
  }

  // На время скачивания закрываем SQLite, чтобы файл ушёл без активной записи.
  closeDatabase();

  File file = LittleFS.open(DB_PATH, "r");
  if (!file) {
    reopenDatabase();
    sendError(500, "Не удалось открыть /app.db для экспорта");
    return;
  }

  server.sendHeader("Content-Disposition", "attachment; filename=app.db");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "application/octet-stream");
  file.close();

  reopenDatabase();
}

void handleDbBackupExport() {
  if (!LittleFS.exists(DB_BACKUP_PATH)) {
    sendError(404, "Backup /app_backup.db не найден");
    return;
  }

  File file = LittleFS.open(DB_BACKUP_PATH, "r");
  if (!file) {
    sendError(500, "Не удалось открыть backup для скачивания");
    return;
  }

  server.sendHeader("Content-Disposition", "attachment; filename=app_backup.db");
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(file, "application/octet-stream");
  file.close();
}

void handleDbImportUpload() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    dbUploadOk = true;
    dbUploadError = "";
    dbUploadSize = 0;

    if (!upload.filename.endsWith(".db") && !upload.filename.endsWith(".sqlite")) {
      dbUploadOk = false;
      dbUploadError = "Можно загружать только .db или .sqlite файл";
      return;
    }

    closeDatabase();

    if (LittleFS.exists(DB_IMPORT_TMP_PATH)) LittleFS.remove(DB_IMPORT_TMP_PATH);

    dbUploadFile = LittleFS.open(DB_IMPORT_TMP_PATH, "w");
    if (!dbUploadFile) {
      dbUploadOk = false;
      dbUploadError = "Не удалось создать временный файл импорта";
      reopenDatabase();
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!dbUploadOk || !dbUploadFile) return;

    size_t written = dbUploadFile.write(upload.buf, upload.currentSize);
    dbUploadSize += written;

    if (written != upload.currentSize) {
      dbUploadOk = false;
      dbUploadError = "Ошибка записи файла в LittleFS";
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (dbUploadFile) dbUploadFile.close();

    if (!dbUploadOk) {
      LittleFS.remove(DB_IMPORT_TMP_PATH);
      reopenDatabase();
      return;
    }

    if (dbUploadSize == 0) {
      dbUploadOk = false;
      dbUploadError = "Загруженный файл пустой";
      LittleFS.remove(DB_IMPORT_TMP_PATH);
      reopenDatabase();
      return;
    }

    if (!validateImportedDatabase("/littlefs/app_import.tmp")) {
      dbUploadOk = false;
      dbUploadError = "Файл не похож на базу этой админки: не найдены нужные таблицы";
      LittleFS.remove(DB_IMPORT_TMP_PATH);
      reopenDatabase();
      return;
    }

    // Сохраняем текущую базу в backup перед заменой.
    if (LittleFS.exists(DB_PATH)) {
      LittleFS.remove(DB_BACKUP_PATH);
      if (!LittleFS.rename(DB_PATH, DB_BACKUP_PATH)) {
        dbUploadOk = false;
        dbUploadError = "Не удалось создать backup старой базы";
        LittleFS.remove(DB_IMPORT_TMP_PATH);
        reopenDatabase();
        return;
      }
    }

    if (!LittleFS.rename(DB_IMPORT_TMP_PATH, DB_PATH)) {
      dbUploadOk = false;
      dbUploadError = "Не удалось заменить app.db новым файлом";
      if (LittleFS.exists(DB_BACKUP_PATH) && !LittleFS.exists(DB_PATH)) {
        LittleFS.rename(DB_BACKUP_PATH, DB_PATH);
      }
      reopenDatabase();
      return;
    }

    if (!reopenDatabase()) {
      dbUploadOk = false;
      dbUploadError = "Новая база загружена, но SQLite не смог её открыть";
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (dbUploadFile) dbUploadFile.close();
    LittleFS.remove(DB_IMPORT_TMP_PATH);
    dbUploadOk = false;
    dbUploadError = "Загрузка файла была прервана";
    reopenDatabase();
  }
}

void handleDbImportDone() {
  if (!dbUploadOk) {
    sendError(500, dbUploadError.length() ? dbUploadError : "Ошибка импорта базы");
    return;
  }

  String json = "{\"ok\":true";
  json += ",\"message\":\"База импортирована\"";
  json += ",\"size\":" + String(dbUploadSize);
  json += "}";
  sendJson(200, json);
}

void handleDbRestoreBackup() {
  if (!LittleFS.exists(DB_BACKUP_PATH)) {
    sendError(404, "Backup /app_backup.db не найден");
    return;
  }

  closeDatabase();

  if (LittleFS.exists(DB_PATH)) LittleFS.remove(DB_PATH);

  if (!LittleFS.rename(DB_BACKUP_PATH, DB_PATH)) {
    reopenDatabase();
    sendError(500, "Не удалось восстановить backup");
    return;
  }

  if (!reopenDatabase()) {
    sendError(500, "Backup восстановлен, но SQLite не смог открыть базу");
    return;
  }

  sendOk();
}

// ======================= Routes =======================
void setupRoutes() {

  server.on("/", HTTP_GET, handleRoot);
  server.on("/index.html", HTTP_GET, handleRoot);
  server.on("/style.css", HTTP_GET, []() { handleStaticFile("/style.css", "text/css; charset=utf-8"); });
  server.on("/bootstrap.min.css", HTTP_GET, []() { handleStaticFile("/bootstrap.min.css", "text/css; charset=utf-8"); });
  server.on("/db.html", HTTP_GET, []() { handleStaticFile("/db.html","text/html; charset=utf-8"); });
  server.on("/db-admin.html", HTTP_GET, []() { handleStaticFile("/db-admin.html","text/html; charset=utf-8"); });
  server.on("/app.js", HTTP_GET, []() { handleStaticFile("/app.js", "application/javascript; charset=utf-8"); });
  server.on("/bootstrap.bundle.min.js", HTTP_GET, []() { handleStaticFile("/bootstrap.bundle.min.js", "application/javascript; charset=utf-8"); });
  server.on("/db.js", HTTP_GET, []() { handleStaticFile("/db.js", "application/javascript; charset=utf-8"); });
  server.on("/db-admin.js", HTTP_GET, []() { handleStaticFile("/db-admin.js", "application/javascript; charset=utf-8"); });
  server.on("/api/devices", HTTP_GET, handleGetDevices);
  server.on("/api/devices/save", HTTP_POST, handleSaveDevice);
  server.on("/api/devices/delete", HTTP_POST, handleDeleteDevice);

  server.on("/api/functions", HTTP_GET, handleGetFunctions);
  server.on("/api/functions/save", HTTP_POST, handleSaveFunction);
  server.on("/api/functions/delete", HTTP_POST, handleDeleteFunction);

  server.on("/api/receive-functions", HTTP_GET, handleGetReceiveFunctions);
  server.on("/api/receive-functions/save", HTTP_POST, handleSaveReceiveFunction);
  server.on("/api/receive-functions/delete", HTTP_POST, handleDeleteReceiveFunction);

  server.on("/api/algorithms", HTTP_GET, handleGetAlgorithms);
  server.on("/api/algorithms/save", HTTP_POST, handleSaveAlgorithm);
  server.on("/api/algorithms/delete", HTTP_POST, handleDeleteAlgorithm);
  server.on("/api/algorithms/run", HTTP_POST, handleRunAlgorithm);
  server.on("/api/logs", HTTP_GET, handleGetLogs);
  server.on("/api/logs/add", HTTP_POST, handleAddLog);
  server.on("/api/logs/clear", HTTP_POST, handleClearLogs);

  server.on("/api/debug/start", HTTP_POST, handleDebugStart);

  server.on("/api/db/info", HTTP_GET, handleDbInfo);
  server.on("/api/db/tables", HTTP_GET, handleDbTables);
  server.on("/api/db/table", HTTP_GET, handleDbTableData);
  server.on("/api/db/query", HTTP_POST, handleDbQuery);
  server.on("/api/db/table/save", HTTP_POST, handleDbTableSave);
  server.on("/api/db/table/delete", HTTP_POST, handleDbTableDelete);
  server.on("/api/db/export", HTTP_GET, handleDbExport);
  server.on("/api/db/export-backup", HTTP_GET, handleDbBackupExport);
  server.on("/api/db/import", HTTP_POST, handleDbImportDone, handleDbImportUpload);
  server.on("/api/db/restore-backup", HTTP_POST, handleDbRestoreBackup);
  server.onNotFound([]()
  {
    sendError(404, "Маршрут не найден: " + server.uri());
  });
}




void device_init()
{
  //Serial.println("Start device init");
  //SMB100A_geterodin_client = WiFiClient();
  client_SM100A   = new  WiFiClient();
  client_RFSU40   = new  WiFiClient();
  client_FSW26    = new  WiFiClient();
  client_PRF7100L = new  WiFiClient(); 
  client_MX4      = new  WiFiClient();
  client_MX6      = new  WiFiClient(); 
  client          = new  WiFiClient(); 

  Device.ID = "Device";
  Device.client = client;


  MX_6.ID = "MX_2SP6T_0018";
  MX_6.client = client_MX6;
  MX_6.IP_add = IP_MX_2SP6T_0018;
  MX_6.Port = port_MX_2SP6T_0018;

  MX_4.ID = "MX_2SP4T_0018";
  MX_4.client = client_MX4;
  MX_4.IP_add = IP_MX_2SP4T_0018;
  MX_4.Port = port_MX_2SP4T_0018;

  Generator_get.ID = "RFSU40_geterodin";
  Generator_get.client = client_RFSU40;
  Generator_get.IP_add = IP_RFSU40;
  Generator_get.Port = port_RFSU40;

  Generator.ID = "SMB100A";
  Generator.client = client_SM100A;
  Generator.IP_add = IP_SMB100A;
  Generator.Port = port_SMB100A;

  DC_source.ID = "PFR_7100L";
  DC_source.client = client_PRF7100L;
  DC_source.IP_add = IP_PRF7100L;
  DC_source.Port = port_PRF7100L;

  Spectr_Analyzer.ID = "FSW26";
  Spectr_Analyzer.client = client_FSW26;
  Spectr_Analyzer.IP_add =IP_FSW26;
  Spectr_Analyzer.Port = port_FSW26;

  //Serial.println("Stop device init");
}

String send_read_(IP_Port_Device *dev , String str)//основной обработчик запрос ответ
{
  if (dev->client->connect(dev->IP_add, dev->Port))
  {     
    if(dev->client->connected())
    {      
      dev->client->write(str.c_str());    
    }   
    unsigned long timeout = 1000;
    unsigned long startt=millis();
    while((millis()-startt) < timeout)
    {
      if (dev->client->available()> 0)
      {
        str = dev->client->readString();        
        break;
      } 
    }
    dev->client->stop();   
  }
  return str;
}

String send_read(IP_Port_Device *dev , String str)
{ 
  if((dev->Port == port_MX_2SP4T_0018) || (dev->Port == port_MX_2SP6T_0018))
  {
    return send_read_(dev, str);//без конца строки для китайчат 
  }
  else
  {
    return send_read_(dev, (str + "\r\n"));//добавляем конец строки 
  }   
}

String send_read_MX(IP_Port_Device *dev, String str)
{
  return send_read_(dev, str);  
}

void IDN_substring(IP_Port_Device *dev, String str)//для всех нормальных приборов
{
  if((str == "*IDN?\r\n")||(str == "*idn?\r\n"))//прибор не определен
  {
    dev->identificator = false;//прибор не определен
  }
  else
  {
    dev->identificator = true;
    //поиск запятых в идентификатре
    int index1 = str.indexOf(',');
    int index2 = str.indexOf(',', index1+1);
    int index3 = str.indexOf(',', index2+1);
    //выделение подстрок в идентификаторе 
    dev->manufacture   = str.substring( 0,         index1);//изготовитель
    dev->instrument    = str.substring( index1+1,  index2);//прибор
    dev->ser_num       = str.substring( index2+1,  index3);//серийный номер
    dev->soft_ver      = str.substring( index3+1         );//версия ПО
  }
}

void IDN_MX_substring(IP_Port_Device *dev, String str)//для китайского поделия
{
  if(str == "*IDN?")//прибор не определен только ЗАГЛАВНЫЕ
  {
    dev->identificator = false;//прибор не определен
  }
  else  //MX-2SP6T-0018
  {
    dev->identificator = true;
    //поиск запятых в идентификатре
    int index1 = str.indexOf('-');
    int index2 = str.indexOf('-', index1+1);
   
    //выделение подстрок в идентификаторе 
    dev->manufacture   = str.substring( 0,         index1);//изготовитель
    dev->instrument    = str.substring( index1+1,  index2);//прибор
    dev->ser_num       = str.substring( index2+1         );//серийный номер    
  }
}

String send_IDN(IP_Port_Device *dev)
{
  String str = "*IDN?";
  str  = send_read(dev, str);
  IDN_substring(dev, str); 
  return str;
}

String send_IDN_MX(IP_Port_Device *dev)
{
  String str = "*IDN?";
  str  = send_read_MX(dev, str);
  IDN_MX_substring(dev, str); 
  return str;
}

TaskHandle_t TaskHandleSMB100A_geterodin = NULL;


void taskSMB100A_geterodin(void * parameter)
{
  // ВАЖНО: задача запускается только после setupRoutes() и server.begin()
  for (;;)
  {
    //server.handleClient();
    vTaskDelay(1);
  }
}


// ======================= Arduino setup/loop =======================
void setup() 
{  

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  display.clear();

  PIN_OUT_INIT();

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("Start ESP32 Admin DB W5500...");

  if (!LittleFS.begin(true)) {
    Serial.println("Ошибка LittleFS");
    return;
  }
  Serial.println("LittleFS запущен");

  if (!initDatabase()) {
    Serial.println("Ошибка инициализации базы");
    return;
  }
  Serial.println("SQLite база готова");

  device_init();

  ESP32_W5500_onEvent();

  if (!ETH.begin(MISO_GPIO, MOSI_GPIO, SCK_GPIO, CS_GPIO, INT_GPIO, SPI_CLOCK_MHZ, ETH_SPI_HOST, mac[millis() % NUMBER_OF_MAC])) {
    Serial.println("ETH begin failed");
    while (true) delay(1000);
  }

  // Для DHCP закомментируй следующую строку:
  ETH.config(myIP, myGW, mySN, myDNS);

  ESP32_W5500_waitForConnect();
  Serial.print("IP: ");
  Serial.println(ETH.localIP());

  setupRoutes();

  server.begin();
  Serial.println("HTTP Server started");

  /*xTaskCreate(
    taskSMB100A_geterodin,
    "http_server_task",
    1024 * 16,
    NULL,
    1,
    &TaskHandleSMB100A_geterodin
  );*/
  Serial.println("HTTP task started");
}

void loop() 
{
  server.handleClient();
  runActiveAlgorithmsIfNeeded();
  vTaskDelay(1);
}

// Загрузка прошивки:    pio run -t upload
// Загрузка интерфейса:  pio run -t uploadfs
