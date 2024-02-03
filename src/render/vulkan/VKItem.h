#pragma once

#include "Singletons.h"
#include "session/SessionModel.h"
#include "MessageList.h"
#include "FileDialog.h"
#include "FileCache.h"
#include "KDGpuEnums.h"

// prevent COM from polluting global scope
#if defined(interface)
#  undef interface
#endif
