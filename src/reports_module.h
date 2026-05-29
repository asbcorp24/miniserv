#pragma once

#include <Arduino.h>

extern "C" {
  #include "lua.h"
}

void initReportDatabase();
void registerReportHttpRoutes();
void registerReportLuaFunctions(lua_State* L);
