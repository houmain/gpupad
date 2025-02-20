
#include "dllreflect_export.h"

int test(int a, int b) {
  return a + b;
}

DLLREFLECT_BEGIN
DLLREFLECT_FUNC(test)
DLLREFLECT_END
