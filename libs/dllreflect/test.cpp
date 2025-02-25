
#include "include/dllreflect.h"
#include <memory>
#include <cassert>

#define assert_throws(X) try { X; assert(!"no exception thrown"); } catch (...) { }

//---------------------------------------------

namespace test {
  struct A;
  struct B;

  void testVoid() {
  }

  int32_t addIntUint(int32_t x, uint32_t y) {
    return x + y;
  }

  void testAA(A* a, A* b) {
  }

  void testAB(A* a, B* b) {
  }

  std::string makeString() {
    return "hello";
  }

  std::string makeEmptyString() {
    return "";
  }

  int32_t useConstStringRef(const std::string& string) {
    return static_cast<int32_t>(string.size());
  }

  int32_t useConstCharPtr(const char* string) {
    return static_cast<int32_t>(std::strlen(string));
  }

  int32_t useStringView(std::string_view string) {
    return static_cast<int32_t>(string.size());
  }

  using MyOpaqueType = std::unique_ptr<std::string>;

  MyOpaqueType makeOpaque() {
    return std::make_unique<std::string>("hello");
  }

  uint32_t useConstOpaqueRef(const MyOpaqueType& data) {
    return static_cast<uint32_t>(data->size());
  }

  void useOpaqueRef(MyOpaqueType& data) {
    *data += "A";
  }

  uint32_t sinkOpaque(MyOpaqueType&& data) {
    return static_cast<uint32_t>(data->size());
  }
} // namespace

// simulate DLL export
DLLREFLECT_BEGIN(using namespace test)
DLLREFLECT_FUNC(addIntUint)
DLLREFLECT_FUNC(makeString)
DLLREFLECT_FUNC(makeEmptyString)
DLLREFLECT_FUNC(useConstStringRef)
DLLREFLECT_FUNC(useConstCharPtr)
DLLREFLECT_FUNC(useStringView)
DLLREFLECT_FUNC(testAA)
DLLREFLECT_FUNC(testAB)
DLLREFLECT_FUNC(makeOpaque)
DLLREFLECT_FUNC(useConstOpaqueRef)
//DLLREFLECT_FUNC(useOpaqueRef)
//DLLREFLECT_FUNC(sinkOpaque)
DLLREFLECT_END()

//---------------------------------------------

int main() {
  // everything on export side is constexpr
  constexpr auto testVoid = dllreflect::describe("testVoid", &test::testVoid);
  static_assert(testVoid.argument_count == 0);
  static_assert(testVoid.result_type == dllreflect::Type::Void);

  constexpr auto addIntUint = dllreflect::describe("addIntUint", &test::addIntUint);
  static_assert(addIntUint.argument_count == 2);
  static_assert(addIntUint.argument_types[0] == dllreflect::Type::Int32);
  static_assert(addIntUint.argument_types[1] == dllreflect::Type::UInt32);

  constexpr auto makeString = dllreflect::describe("makeString", &test::makeString);
  static_assert(makeString.result_type == 
    (dllreflect::Type::Utf8Codepoint | dllreflect::TypeFlags::array));

  constexpr auto useConstStringRef = dllreflect::describe(
    "useConstStringRef", &test::useConstStringRef);
  static_assert(useConstStringRef.argument_types[0] == 
    (dllreflect::Type::Utf8Codepoint | dllreflect::TypeFlags::array));

  // type safety for opaque types
  constexpr auto testAA = dllreflect::describe("testAA", &test::testAA);
  static_assert(testAA.argument_types[0] == testAA.argument_types[1]);
  constexpr auto testAB = dllreflect::describe("testAB", &test::testAB);
  static_assert(testAB.argument_types[0] != testAB.argument_types[1]);

  // simulate DLL import
  const auto& interface = *dllreflect_export();
  const auto get_function = [&](const char* name) -> const dllreflect::Function& {
    for (auto i = 0u; i < interface.function_count; ++i) {
      const auto& function = interface.functions[i];
      if (!std::strcmp(function.name, name))
        return function;
    }
    throw std::runtime_error("invalid function");
  };

  // calling exported functions
  dllreflect::call<void>(testVoid);
  assert(dllreflect::call<int32_t>(addIntUint, int32_t{ 100 }, uint32_t{ 200 }) == 300);
  assert_throws(dllreflect::call<int32_t>(addIntUint, int32_t{ }));
  assert_throws(dllreflect::call<int32_t>(addIntUint, int32_t{ }, int32_t{ }));
  assert_throws(dllreflect::call<uint32_t>(addIntUint, int32_t{ }, uint32_t{ }));

  assert(dllreflect::call<std::string>(makeString) == "hello");
  assert(dllreflect::call<int32_t>(useConstStringRef, std::string("hello")) == 5);
  const auto makeEmptyString = get_function("makeEmptyString");
  const auto useConstCharPtr = get_function("useConstCharPtr");
  const auto useStringView = get_function("useStringView");
  assert(dllreflect::call<std::string>(makeEmptyString) == "");
  assert(dllreflect::call<int32_t>(useConstStringRef, "ab") == 2);
  assert(dllreflect::call<int32_t>(useConstStringRef, std::string_view("abc")) == 3);
  assert(dllreflect::call<int32_t>(useConstCharPtr, "ab") == 2);
  assert(dllreflect::call<int32_t>(useConstCharPtr, std::string_view("abc")) == 3);
  assert(dllreflect::call<int32_t>(useStringView, "ab") == 2);
  assert(dllreflect::call<int32_t>(useStringView, std::string_view("abc")) == 3);

  const auto makeOpaque = get_function("makeOpaque");
  const auto useConstOpaqueRef = get_function("useConstOpaqueRef");
  //const auto useOpaqueRef = get_function("useOpaqueRef");
  //const auto sinkOpaque = get_function("sinkOpaque");
  dllreflect::Opaque opaque = dllreflect::call<dllreflect::Opaque>(makeOpaque);
  assert(dllreflect::call<uint32_t>(useConstOpaqueRef, opaque) == 5);
  assert_throws(dllreflect::call<uint32_t>(useConstOpaqueRef, dllreflect::Opaque()));
  assert_throws(dllreflect::call<uint32_t>(useConstOpaqueRef, std::make_unique<int>()));
  //dllreflect::call<void>(useOpaqueRef, opaque);
  //dllreflect::call<void>(useOpaqueRef, opaque);
  //assert(dllreflect::call<uint32_t>(useConstOpaqueRef, opaque) == 7);
  //assert_throws(dllreflect::call<uint32_t>(sinkOpaque, opaque));
  //assert(dllreflect::call<uint32_t>(sinkOpaque, std::move(opaque)) == 7);
}
