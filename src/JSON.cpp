#include "JSON.h"

#include "Settings.h"
#include "Singletons.h"

#include <QMetaType>
#include <QStringList>

const JsonValue *findJsonValue(const JsonObject &object, const std::string &key)
{
    const auto it = object.find(key);
    return (it == object.end() ? nullptr : &it->second);
}

const JsonObject *jsonObject(const JsonValue &value)
{
    return value.get_ptr<const JsonObject *>();
}

const JsonArray *jsonArray(const JsonValue &value)
{
    return value.get_ptr<const JsonArray *>();
}

const JsonArray *jsonArray(const JsonObject &object, const std::string &key)
{
    const auto value = findJsonValue(object, key);
    return (value ? jsonArray(*value) : nullptr);
}

std::string jsonKey(const QString &value)
{
    const auto utf8 = value.toUtf8();
    return { utf8.constData(), static_cast<std::size_t>(utf8.size()) };
}

QString jsonString(const JsonValue &value)
{
    return value.get<QString>();
}

JsonValue jsonFromVariant(const QVariant &value)
{
    if (!value.isValid() || value.isNull())
        return nullptr;

    switch (value.typeId()) {
    case QMetaType::Bool:        return value.toBool();
    case QMetaType::Char:
    case QMetaType::SChar:
    case QMetaType::Short:
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::LongLong:    return value.toLongLong();
    case QMetaType::UChar:
    case QMetaType::UShort:
    case QMetaType::UInt:
    case QMetaType::ULong:
    case QMetaType::ULongLong:   return value.toULongLong();
    case QMetaType::Float:
    case QMetaType::Double:      return value.toDouble();
    case QMetaType::QString:     return value.toString();
    case QMetaType::QStringList: {
        auto result = JsonArray();
        for (const auto &item : value.toStringList())
            result.push_back(item);
        return result;
    }
    case QMetaType::QVariantList: {
        auto result = JsonArray();
        for (const auto &item : value.toList())
            result.push_back(jsonFromVariant(item));
        return result;
    }
    case QMetaType::QVariantMap: {
        auto result = JsonObject();
        const auto map = value.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            result[jsonKey(it.key())] = jsonFromVariant(it.value());
        return result;
    }
    case QMetaType::QVariantHash: {
        auto result = JsonObject();
        const auto hash = value.toHash();
        for (auto it = hash.cbegin(); it != hash.cend(); ++it)
            result[jsonKey(it.key())] = jsonFromVariant(it.value());
        return result;
    }
    default: return value.toString();
    }
}

QVariant variantFromJson(const JsonValue &value)
{
    if (value.is_null() || value.is_discarded())
        return {};
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return QVariant::fromValue(value.get<qlonglong>());
    if (value.is_number_unsigned())
        return QVariant::fromValue(value.get<qulonglong>());
    if (value.is_number_float())
        return value.get<double>();
    if (value.is_string())
        return jsonString(value);
    if (value.is_array()) {
        auto result = QVariantList();
        result.reserve(static_cast<qsizetype>(value.size()));
        for (const auto &item : value)
            result.push_back(variantFromJson(item));
        return result;
    }
    if (value.is_object()) {
        auto result = QVariantMap();
        for (auto it = value.cbegin(); it != value.cend(); ++it)
            result[QString::fromUtf8(it.key().data(),
                static_cast<qsizetype>(it.key().size()))] =
                variantFromJson(it.value());
        return result;
    }
    return {};
}

JsonValue parseJson(const QString &text)
{
    const auto utf8 = text.toUtf8();
    return JsonValue::parse(utf8.constData(), utf8.constData() + utf8.size(),
        nullptr, false);
}

QString serializeJson(const JsonValue &json)
{
    const auto &settings = Singletons::settings();
    const auto indentWithSpaces = settings.indentWithSpaces();
    auto text = json.dump(indentWithSpaces ? settings.tabSize() : 1,
        indentWithSpaces ? ' ' : '\t');
    text.push_back('\n');
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
