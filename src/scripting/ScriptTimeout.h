#pragma once

#include <chrono>
class ScriptEngine;

void setScriptEngineTimeout(std::chrono::milliseconds timeout);
void registerRunningScriptEngine(ScriptEngine *scriptEngine);
void deregisterRunningScriptEngine(ScriptEngine *scriptEngine);
void interruptRunningScriptEngines();
