
#include "LibraryScriptObject.h"
#include <QJSEngine>

#define DLLREFLECT_IMPORT_IMPLEMENTATION
#include "dllreflect/include/dllreflect_import.h"

namespace {
    class OpaqueArgument : public QObject
    {
    public:
        OpaqueArgument(std::shared_ptr<void> library,
            dllreflect::Argument &&argument, QObject *parent = nullptr)
            : QObject(parent)
            , mLibrary(std::move(library))
            , mArgument(std::move(argument))
        {
        }

        ~OpaqueArgument()
        {
            if (mArgument.free)
                mArgument.free(mArgument.data);
        }

        const dllreflect::Argument &argument() const { return mArgument; }

        bool checkType(dllreflect::ArgumentType &type) const
        {
            return type.opaque_id == mArgument.type.opaque_id;
        }

        void release() { mArgument = {}; }

    private:
        std::shared_ptr<void> mLibrary;
        dllreflect::Argument mArgument;
    };

    template <typename T, typename S>
    void writeValue(void *data, const S &value)
    {
        *static_cast<T *>(data) = static_cast<T>(value);
    }

    void copyString(dllreflect::Argument &argument, const QString &string)
    {
        Q_ASSERT(argument.type.type == dllreflect::Type::Utf8Codepoint);
        const auto utf8 = string.toUtf8();
        argument.count = utf8.size() + 1;
        argument.data = new char[argument.count];
        argument.free = [](void *data) { delete[] data; };
        std::memcpy(argument.data, utf8.constData(), argument.count);
    }

    QJSValue copyString(const dllreflect::Argument &argument)
    {
        if (argument.count == 0)
            return {};
        return QString::fromUtf8(static_cast<const char *>(argument.data),
            argument.count - 1);
    }
} // namespace

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
        mEngine->throwError(
            QStringLiteral("invalid argument count (%1 provided, %2 expected)")
                .arg(arguments.size())
                .arg(function.argument_count));
        return {};
    }

    const auto getArgumentSize = [&](size_t index, dllreflect::Type type) {
        switch (type) {
        case dllreflect::Type::Void:
        case dllreflect::Type::Opaque:
        case dllreflect::Type::Utf8Codepoint: return 0;
        default:                              return 1;
        }
    };

    const auto getArgument = [&](size_t index, dllreflect::Argument &argument) {
        auto ok = false;
        switch (argument.type.type) {
#define ADD(TYPE, T, GET) \
    case TYPE: writeValue<T>(argument.data, arguments[index].GET(&ok)); break;
            ADD(dllreflect::Type::Bool, bool, toUInt)
            ADD(dllreflect::Type::Char, char, toUInt)
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

        case dllreflect::Type::Opaque:
            if (auto opaque = qvariant_cast<OpaqueArgument *>(arguments[index]);
                opaque && opaque->checkType(argument.type)) {

                argument = opaque->argument();
                // TODO: do not always move
                opaque->release();
                ok = true;
            }
            break;

        case dllreflect::Type::Utf8Codepoint:
            copyString(argument, arguments[index].toString());
            ok = true;
            break;
        }
        if (!ok)
            mEngine->throwError(
                QStringLiteral("conversion of argument %1 failed")
                    .arg(index + 1));
        return ok;
    };

    auto result = QJSValue{};
    const auto writeResult = [&](dllreflect::Argument &argument) {
        auto ok = false;
        switch (argument.type.type) {

#define ADD(TYPE, T)                                               \
    case TYPE:                                                     \
        result = QJSValue(*static_cast<const T *>(argument.data)); \
        ok = true;                                                 \
        break;

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
            // QJSValue does not have a qlonglong constructor, yet?
            ADD_CHECKED(dllreflect::Type::Int64, int64_t, int)
            ADD_CHECKED(dllreflect::Type::UInt64, uint64_t, uint)
            ADD(dllreflect::Type::Float, float)
            ADD(dllreflect::Type::Double, double)
#undef ADD

        case dllreflect::Type::Void: ok = true; break;

        case dllreflect::Type::Opaque:
            result = mEngine->newQObject(
                new OpaqueArgument(mLibrary, std::exchange(argument, {})));
            ok = true;
            break;

        case dllreflect::Type::Utf8Codepoint:
            result = copyString(argument);
            ok = result.isString();
            break;
        }
        if (!ok)
            mEngine->throwError(QJSValue::GenericError,
                "conversion of result failed");
        return ok;
    };
    if (!dllreflect::make_call(function, getArgumentSize, getArgument,
            writeResult))
        return {};
    return result;
}

//-------------------------------------------------------------------------

LibraryScriptObject::LibraryScriptObject(QObject *parent)
    : QQmlPropertyMap(parent)
{
}

bool LibraryScriptObject::load(QJSEngine *engine, const QString &fileName)
{
    setObjectName(fileName);
    auto library = std::make_unique<dllreflect::Library>();

    const auto directory = QString(".");
#if !defined(_WIN32)
    if (!library->load(qUtf8Printable(directory), qUtf8Printable(fileName)))
        return false;
#else
    if (!library->load(qUtf16Printable(directory), qUtf16Printable(fileName)))
        return false;
#endif

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
