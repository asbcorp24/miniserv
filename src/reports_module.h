#pragma once

#include <Arduino.h>

extern "C" {
  #include "lua.h"
  #include "lualib.h"
  #include "lauxlib.h"
}

void initReportDatabase();
void registerReportHttpRoutes();
void registerReportLuaFunctions(lua_State* L);

// Вызывать в основном loop(), чтобы WebSocket уведомления отчётов работали.
void reportWebSocketLoop();
