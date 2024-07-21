#pragma once

#include "FileCache.h"
#include "FileDialog.h"
#include "KDGpuEnums.h"
#include "MessageList.h"
#include "Singletons.h"
#include "session/SessionModel.h"

// prevent COM from polluting global scope
#if defined(interface)
#  undef interface
#endif
