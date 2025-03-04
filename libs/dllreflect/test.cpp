
#include "include/dllreflect.h"
#include <memory>
#include <cassert>
#include <numeric>

#define assert_throws(X) try { X; assert(!"no exception thrown"); } catch (...) { }

//---------------------------------------------

struct A { int x; };
struct B { int y; };

namespace test {
  void testVoid() {
  }

  int32_t addIntUint(int32_t x, uint32_t y) {
    return x + y;
  }

  std::string makeString() {
    return "hello";
  }

  const char* makeStringLiteral() {
    return "hello";
  }

  std::string_view makeStringView() {
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

  int* makeOpaquePointer() {
    static int s_int = 5;
    return &s_int;
  }

  int32_t useOpaquePointer(int* value) {
    return (*value)++;
  }

  std::vector<A> makeOpaqueVector() {
    return { { 1 }, { 2 }, { 3 } };
  }

  std::vector<float> getFloatVector() {
    return { 1, 2, 3, 4, 5 };
  }

  dllreflect::span<const float> getConstFloatSpan() {
    static const float s_floats[] { 1, 2, 3, 4, 5 };
    return { std::begin(s_floats), std::end(s_floats) };
  }

  float sumFloatVector(std::vector<float> values) {
    return std::accumulate(values.begin(), values.end(), 0.0f);
  }

  float sumConstFloatSpan(dllreflect::span<const float> values) {
    return std::accumulate(values.begin(), values.end(), 0.0f);
  }

  using MyOpaqueType = std::unique_ptr<std::string>;

  MyOpaqueType makeOpaque() {
    return std::make_unique<std::string>("hello");
  }

  uint32_t useConstOpaqueRef(const MyOpaqueType& data) {
    return static_cast<uint32_t>(data->size());
  }

  void useOpaqueRef(MyOpaqueType& data) {
    *data += "x";
  }
} // namespace

// simulate DLL export
DLLREFLECT_BEGIN(using namespace test)
DLLREFLECT_FUNC(addIntUint)
DLLREFLECT_FUNC(makeString)
DLLREFLECT_FUNC(makeStringLiteral)
DLLREFLECT_FUNC(makeStringView)
DLLREFLECT_FUNC(makeEmptyString)
DLLREFLECT_FUNC(useConstStringRef)
DLLREFLECT_FUNC(useConstCharPtr)
DLLREFLECT_FUNC(useStringView)
DLLREFLECT_FUNC(getFloatVector)
DLLREFLECT_FUNC(getConstFloatSpan)
DLLREFLECT_FUNC(sumFloatVector)
DLLREFLECT_FUNC(sumConstFloatSpan)
DLLREFLECT_FUNC(makeOpaque)
DLLREFLECT_FUNC(useConstOpaqueRef)
DLLREFLECT_FUNC(useOpaqueRef)
DLLREFLECT_END()

//---------------------------------------------

int main() {
  using namespace dllreflect;

  // everything on export side is constexpr
  constexpr auto testVoid = describe("testVoid", &test::testVoid);
  static_assert(testVoid.argument_count == 0);
  static_assert(testVoid.result_type == Type::Void);

  constexpr auto addIntUint = describe("addIntUint", &test::addIntUint);
  static_assert(addIntUint.argument_count == 2);
  static_assert(addIntUint.argument_types[0] == Type::Int32);
  static_assert(addIntUint.argument_types[1] == Type::UInt32);

  constexpr auto makeString = describe("makeString", &test::makeString);
  static_assert(makeString.result_type == 
    (Type::Char | TypeFlags::array));

  constexpr auto makeStringLiteral = describe("makeStringLiteral",
    &test::makeStringLiteral);
  static_assert(makeStringLiteral.result_type ==
    (Type::Char | TypeFlags::array | TypeFlags::view));

  constexpr auto makeStringView = describe("makeStringView",
    &test::makeStringView);
  static_assert(makeStringView.result_type == 
    (Type::Char | TypeFlags::array | TypeFlags::view));

  constexpr auto useConstStringRef = describe(
    "useConstStringRef", &test::useConstStringRef);
  static_assert(useConstStringRef.argument_types[0] == 
    (Type::Char | TypeFlags::array));

  assert(call<std::string>(makeString) == "hello");
  assert(call<std::string>(makeStringLiteral) == "hello");
  assert(call<std::string>(makeStringView) == "hello");
  assert_throws(call<std::string_view>(makeString));
  assert(call<std::string_view>(makeStringLiteral) == "hello");
  assert(call<std::string_view>(makeStringView) == "hello");

  // type safety for opaque types
  constexpr auto testAA = describe("testAA", +[](A&, A&) { });
  static_assert(testAA.argument_types[0] == testAA.argument_types[1]);
  constexpr auto testAB = describe("testAB", +[](A&, B&) { });
  static_assert(testAB.argument_types[0] != testAB.argument_types[1]);
  constexpr auto testAA2 = describe("testAA2", +[](A&, A*) { });
  static_assert(testAA2.argument_types[0] != testAA2.argument_types[1]);
  constexpr auto testAA5 = describe("testAA3", +[](A&, A) {});
  static_assert(testAA5.argument_types[0] == testAA5.argument_types[1]);
  // no const correctness yet for opaque types
  constexpr auto testAA4 = describe("testAA4", +[](A&, const A&) {});
  static_assert(testAA4.argument_types[0] == testAA4.argument_types[1]);

  constexpr auto makeOpaquePointer = describe(
    "makeOpaquePointer", &test::makeOpaquePointer);
  static_assert(is_opaque(makeOpaquePointer.result_type));
  constexpr auto useOpaquePointer = describe(
    "useOpaquePointer", &test::useOpaquePointer);
  static_assert(is_opaque(useOpaquePointer.argument_types[0]));
  auto p = call<Opaque>(makeOpaquePointer);
  assert(call<int32_t>(useOpaquePointer, p) == 5);
  assert(call<int32_t>(useOpaquePointer, p) == 6);

  constexpr auto makeOpaqueVector = describe(
    "makeOpaqueVector", &test::makeOpaqueVector);
  static_assert(is_opaque(makeOpaqueVector.result_type) &&
    !(makeOpaqueVector.result_type & TypeFlags::array));

  // simulate DLL import
  const auto& interface = *dllreflect_export();
  const auto get_function = [&](const char* name) -> const Function& {
    for (auto i = 0u; i < interface.function_count; ++i) {
      const auto& function = interface.functions[i];
      if (!std::strcmp(function.name, name))
        return function;
    }
    throw std::runtime_error("invalid function");
  };

  // calling exported functions
  call<void>(testVoid);
  assert(call<int32_t>(addIntUint, int32_t{ 100 }, uint32_t{ 200 }) == 300);
  assert_throws(call<int32_t>(addIntUint, int32_t{ }));
  assert_throws(call<int32_t>(addIntUint, int32_t{ }, int32_t{ }));
  assert_throws(call<uint32_t>(addIntUint, int32_t{ }, uint32_t{ }));

  assert(call<std::string>(makeString) == "hello");
  const auto makeEmptyString = get_function("makeEmptyString");
  const auto useConstCharPtr = get_function("useConstCharPtr");
  const auto useStringView = get_function("useStringView");
  assert(call<std::string>(makeEmptyString) == "");
  assert(call<int32_t>(useConstStringRef, "ab") == 2);
  assert(call<int32_t>(useConstStringRef, std::string_view("abc")) == 3);
  assert(call<int32_t>(useConstStringRef, std::string("abcd")) == 4);
  assert(call<int32_t>(useConstCharPtr, "ab") == 2);
  assert(call<int32_t>(useConstCharPtr, std::string_view("abc")) == 3);
  assert(call<int32_t>(useConstCharPtr, std::string("abcd")) == 4);
  assert(call<int32_t>(useStringView, "ab") == 2);
  assert(call<int32_t>(useStringView, std::string_view("abc")) == 3);
  assert(call<int32_t>(useStringView, std::string("abcd")) == 4);

  const auto getFloatVector = get_function("getFloatVector");
  const auto getConstFloatSpan = get_function("getConstFloatSpan");
  const auto sumFloatVector = get_function("sumFloatVector");
  const auto sumConstFloatSpan = get_function("sumConstFloatSpan");
  const auto floatVector = call<std::vector<float>>(getFloatVector);
  assert(floatVector.size() == 5 && floatVector[4] == 5);
  const auto constFloatSpan = call<span<const float>>(getConstFloatSpan);
  assert(constFloatSpan.size() == 5 && constFloatSpan[4] == 5);
  assert_throws(call<span<const float>>(getFloatVector));
  assert(call<float>(sumFloatVector, floatVector) == 15);
  assert(call<float>(sumConstFloatSpan, constFloatSpan) == 15);
  assert(call<float>(sumConstFloatSpan, floatVector) == 15);
  assert(call<float>(sumFloatVector, constFloatSpan) == 15);

  const auto makeOpaque = get_function("makeOpaque");
  const auto useConstOpaqueRef = get_function("useConstOpaqueRef");
  const auto useOpaqueRef = get_function("useOpaqueRef");
  Opaque opaque = call<Opaque>(makeOpaque);
  assert(call<uint32_t>(useConstOpaqueRef, opaque) == 5);
  assert_throws(call<uint32_t>(useConstOpaqueRef, Opaque()));
  // does not compile: 
  //call<uint32_t>(useConstOpaqueRef, std::make_unique<int>());
  call<void>(useOpaqueRef, opaque);
  call<void>(useOpaqueRef, opaque);
  assert(call<uint32_t>(useConstOpaqueRef, opaque) == 7);
}
