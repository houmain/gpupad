
#include "dllreflect_export.h"

int test(int a, int b) {
  return a + b;
}

std::string makeString(std::string name) {
  return "hello " + name;
}

void test2() {
}

struct A {
  int x;
  int y;
};

A makeA(int x, int y) {
  return A{ x, y };
}

int sumA(A a) {
  return a.x + a.y;
}

DLLREFLECT_BEGIN
DLLREFLECT_FUNC(test)
DLLREFLECT_FUNC(test2)
DLLREFLECT_FUNC(makeString)
DLLREFLECT_FUNC(makeA)
DLLREFLECT_FUNC(sumA)
DLLREFLECT_END
