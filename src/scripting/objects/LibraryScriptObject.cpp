
#define DLLREFLECT_IMPORT_IMPLEMENTATION
#include "dllreflect/include/dllreflect.h"
#include "LibraryScriptObject.h"
#include <QJSEngine>

namespace {
    template <typename T, typename S>
    void writeValue(void *data, const S &value)
    {
        *static_cast<T *>(data) = static_cast<T>(value);
    }

    void copyString(dllreflect::Argument &argument, const QString &string)
    {
        const auto utf8 = string.toUtf8();
        argument.count = utf8.size();
        argument.data = new char[argument.count];
        argument.free = [](void *data) { delete[] static_cast<char *>(data); };
        std::memcpy(argument.data, utf8.constData(), argument.count);
    }

    QJSValue copyString(const dllreflect::Argument &argument)
    {
        if (argument.count == 0)
            return {};
        return QString::fromUtf8(static_cast<const char *>(argument.data),
            argument.count);
    }
} // namespace

LibraryScriptObject_Opaque::LibraryScriptObject_Opaque(
    std::shared_ptr<void> library, dllreflect::Argument &&argument,
    QObject *parent)
    : QObject(parent)
    , mLibrary(std::move(library))
    , mArgument(std::move(argument))
{
    setObjectName(QStringLiteral("Opaque%1")
                      .arg(static_cast<int>(mArgument.type)
                          & ~static_cast<int>(dllreflect::TypeFlags::opaque)));
}

LibraryScriptObject_Opaque::~LibraryScriptObject_Opaque()
{
    if (mArgument.free)
        mArgument.free(mArgument.data);
}

bool LibraryScriptObject_Opaque::checkType(dllreflect::Type type) const
{
    return (type == mArgument.type);
}

//-------------------------------------------------------------------------

LibraryScriptObject_Array::LibraryScriptObject_Array(QJSEngine *jsEngine,
    std::shared_ptr<void> library, dllreflect::Argument &&argument,
    QObject *parent)
    : QObject(parent)
    , mJsEngine(jsEngine)
    , mLibrary(std::move(library))
    , mArgument(std::move(argument))
{
    setObjectName("Array");
}

LibraryScriptObject_Array::~LibraryScriptObject_Array()
{
    if (mArgument.free)
        mArgument.free(mArgument.data);
}

QJSValue LibraryScriptObject_Array::toArray() const
{
    auto result = mJsEngine->newArray();
    const auto baseType = base(mArgument.type);
    switch (baseType) {
#define ADD(TYPE, T)                                               \
    case TYPE: {                                                   \
        const auto begin = static_cast<const T *>(mArgument.data); \
        for (auto i = 0u; i < mArgument.count; ++i)                \
            result.setProperty(i, begin[i]);                       \
        break;                                                     \
    }

#define ADD_WRAP(TYPE, T, DEST)                                    \
    case TYPE: {                                                   \
        const auto begin = static_cast<const T *>(mArgument.data); \
        for (auto i = 0u; i < mArgument.count; ++i)                \
            result.setProperty(i, static_cast<DEST>(begin[i]));    \
        break;                                                     \
    }
        ADD(dllreflect::Type::Bool, bool)
        ADD(dllreflect::Type::Char, char)
        ADD(dllreflect::Type::Int8, int8_t)
        ADD(dllreflect::Type::UInt8, uint8_t)
        ADD(dllreflect::Type::Int16, int16_t)
        ADD(dllreflect::Type::UInt16, uint16_t)
        ADD(dllreflect::Type::Int32, int32_t)
        ADD(dllreflect::Type::UInt32, uint32_t)
        // TODO: QJSValue does not have a qlonglong constructor, yet?
        ADD_WRAP(dllreflect::Type::Int64, int64_t, int)
        ADD_WRAP(dllreflect::Type::UInt64, uint64_t, uint)
        ADD(dllreflect::Type::Float, float)
        ADD(dllreflect::Type::Double, double)
#undef ADD
#undef ADD_WRAP
    case dllreflect::Type::Void: break;
    }
    return result;
}

//-------------------------------------------------------------------------

LibraryScriptObject_Callable::LibraryScriptObject_Callable(QJSEngine *engine,
    std::shared_ptr<dllreflect::Library> library, QObject *parent)
    : QObject(parent)
    , mEngine(engine)
    , mLibrary(std::move(library))
{
}

LibraryScriptObject_Callable::~LibraryScriptObject_Callable() = default;

QJSValue LibraryScriptObject_Callable::call(int index, QVariantList arguments)
{
    const auto &interface = mLibrary->interface();
    if (index < 0 || index >= static_cast<int>(interface.function_count))
        return {};

    const auto &function = interface.functions[index];
    if (arguments.size() != function.argument_count) {
        mEngine->throwError(QStringLiteral(
            "invalid argument count to '%1' (%2 provided, %3 expected)")
                                .arg(function.name)
                                .arg(arguments.size())
                                .arg(function.argument_count));
        return {};
    }

    const auto getArgument = [&](size_t index, dllreflect::Argument &argument) {
        if (argument.type & dllreflect::TypeFlags::array) {
            const auto baseType = base(argument.type);
            if (baseType == dllreflect::Type::Char) {
                copyString(argument, arguments[index].toString());
                return true;
            }

            const auto list = arguments[index].toList();
            argument.count = list.size();

            switch (baseType) {
#define ADD(TYPE, T, GET)                                                    \
    case TYPE: {                                                             \
        argument.data = new T[argument.count];                               \
        auto pos = static_cast<T *>(argument.data);                          \
        for (const auto &value : list)                                       \
            *pos++ = value.GET();                                            \
        argument.free = [](void *data) { delete[] static_cast<T *>(data); }; \
        break;                                                               \
    }
                ADD(dllreflect::Type::Bool, bool, toUInt)
                ADD(dllreflect::Type::Char, int8_t, toUInt)
                ADD(dllreflect::Type::Int8, int8_t, toInt)
                ADD(dllreflect::Type::UInt8, uint8_t, toUInt)
                ADD(dllreflect::Type::Int16, int16_t, toInt)
                ADD(dllreflect::Type::UInt16, uint16_t, toUInt)
                ADD(dllreflect::Type::Int32, int32_t, toInt)
                ADD(dllreflect::Type::UInt32, uint32_t, toUInt)
                ADD(dllreflect::Type::Int64, int64_t, toLongLong)
                ADD(dllreflect::Type::UInt64, uint64_t, toULongLong)
                ADD(dllreflect::Type::Float, float, toFloat)
                ADD(dllreflect::Type::Double, double, toDouble)
#undef ADD
            case dllreflect::Type::Void: break;
            }
            return true;
        }

        if (argument.type & dllreflect::TypeFlags::opaque) {
            auto opaque = qvariant_cast<Opaque *>(arguments[index]);
            if (opaque && opaque->checkType(argument.type)) {
                argument = opaque->argument();
                argument.free = nullptr;
                return true;
            }
            mEngine->throwError(
                QStringLiteral("conversion of '%1' %2. argument failed")
                    .arg(function.name)
                    .arg(index + 1));
            return false;
        }

        auto ok = false;
        switch (argument.type) {
#define ADD(TYPE, T, GET)                                        \
    case TYPE: {                                                 \
        writeValue<T>(argument.data, arguments[index].GET(&ok)); \
        break;                                                   \
    }
            ADD(dllreflect::Type::Bool, bool, toUInt)
            ADD(dllreflect::Type::Char, int8_t, toUInt)
            ADD(dllreflect::Type::Int8, int8_t, toInt)
            ADD(dllreflect::Type::UInt8, uint8_t, toUInt)
            ADD(dllreflect::Type::Int16, int16_t, toInt)
            ADD(dllreflect::Type::UInt16, uint16_t, toUInt)
            ADD(dllreflect::Type::Int32, int32_t, toInt)
            ADD(dllreflect::Type::UInt32, uint32_t, toUInt)
            ADD(dllreflect::Type::Int64, int64_t, toLongLong)
            ADD(dllreflect::Type::UInt64, uint64_t, toULongLong)
            ADD(dllreflect::Type::Float, float, toFloat)
            ADD(dllreflect::Type::Double, double, toDouble)
#undef ADD
        case dllreflect::Type::Void: break;
        }
        if (!ok)
            mEngine->throwError(
                QStringLiteral("conversion of '%1' %2. argument failed")
                    .arg(function.name)
                    .arg(index + 1));
        return ok;
    };

    auto result = QJSValue{};
    const auto writeResult = [&](dllreflect::Argument &argument) {
        if (argument.type & dllreflect::TypeFlags::array) {
            const auto baseType = base(argument.type);

            if (baseType == dllreflect::Type::Char) {
                result = copyString(argument);
                return result.isString();
            }
            result = mEngine->newQObject(
                new Array(mEngine, mLibrary, std::exchange(argument, {})));
            return true;
        }

        if (argument.type & dllreflect::TypeFlags::opaque) {
            result = mEngine->newQObject(
                new Opaque(mLibrary, std::exchange(argument, {})));
            return true;
        }

        auto ok = false;
        switch (argument.type) {

#define ADD(TYPE, T)                                               \
    case TYPE: {                                                   \
        result = QJSValue(*static_cast<const T *>(argument.data)); \
        ok = true;                                                 \
        break;                                                     \
    }

#define ADD_CHECKED(TYPE, T, DEST)                                 \
    case TYPE: {                                                   \
        const auto value = *static_cast<const T *>(argument.data); \
        const auto dest = static_cast<DEST>(value);                \
        ok = (value == dest);                                      \
        result = QJSValue(dest);                                   \
        break;                                                     \
    }
            ADD(dllreflect::Type::Bool, bool)
            ADD(dllreflect::Type::Char, char)
            ADD(dllreflect::Type::Int8, int8_t)
            ADD(dllreflect::Type::UInt8, uint8_t)
            ADD(dllreflect::Type::Int16, int16_t)
            ADD(dllreflect::Type::UInt16, uint16_t)
            ADD(dllreflect::Type::Int32, int32_t)
            ADD(dllreflect::Type::UInt32, uint32_t)
            // TODO: QJSValue does not have a qlonglong constructor, yet?
            ADD_CHECKED(dllreflect::Type::Int64, int64_t, int)
            ADD_CHECKED(dllreflect::Type::UInt64, uint64_t, uint)
            ADD(dllreflect::Type::Float, float)
            ADD(dllreflect::Type::Double, double)
#undef ADD
#undef ADD_CHECKED

        case dllreflect::Type::Void: ok = true; break;
        }
        if (!ok)
            mEngine->throwError(QJSValue::GenericError,
                QStringLiteral("conversion of '%1' result failed")
                    .arg(function.name));
        return ok;
    };
    if (!dllreflect::call(function, getArgument, writeResult))
        return {};
    return result;
}

//-------------------------------------------------------------------------

LibraryScriptObject::LibraryScriptObject(QObject *parent)
    : QQmlPropertyMap(parent)
{
}

bool LibraryScriptObject::load(QJSEngine *engine, const QString &fileName,
    const QStringList &searchPaths)
{
    setObjectName(fileName);
    auto library = std::make_unique<dllreflect::Library>();

    for (const auto &searchPath : searchPaths) {
#if !defined(_WIN32)
        if (library->load(qUtf8Printable(searchPath), qUtf8Printable(fileName)))
            break;
#else
        if (library->load(qUtf16Printable(searchPath),
                qUtf16Printable(fileName)))
            break;
#endif
    }
    if (!library->loaded())
        return false;

    // QQmlPropertyMap is needed for populating functions dynamically, which can
    // not have Q_INVOKABLE functions, therefore delegating to separate object
    const auto &interface = library->interface();
    engine->globalObject().setProperty("_lib",
        engine->newQObject(new LibraryScriptObject_Callable(engine,
            std::move(library), this)));

    // capture temporary global _lib in functions which all forward to
    // 'call' with corresponding index
    const auto callMethodScript =
        QStringLiteral("(lib => (...args) => lib.call(%1, [...args]))(_lib)");
    for (auto i = 0u; i < interface.function_count; ++i)
        insert(interface.functions[i].name,
            QVariant::fromValue(engine->evaluate(callMethodScript.arg(i))));

    engine->globalObject().deleteProperty("_lib");
    freeze();

    return true;
}
