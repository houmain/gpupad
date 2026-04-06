#pragma once

#include <chrono>
#include <memory>

class ScriptEngine;

void setScriptEngineTimeout(std::chrono::milliseconds timeout);
std::shared_ptr<void> suspendScriptEngineTimeout();

void registerRunningScriptEngine(ScriptEngine *scriptEngine);
void deregisterRunningScriptEngine(ScriptEngine *scriptEngine);
void interruptRunningScriptEngines();
