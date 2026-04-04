#pragma once

class ScriptEngine;

void registerRunningScriptEngine(ScriptEngine *scriptEngine);
void deregisterRunningScriptEngine(ScriptEngine *scriptEngine);
void interruptRunningScriptEngines();
