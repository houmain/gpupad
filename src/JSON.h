#pragma once

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <nlohmann/json.hpp>
#include <string>

namespace nlohmann {
    template <>
    struct adl_serializer<QString>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &json, const QString &value)
        {
            const auto utf8 = value.toUtf8();
            json = typename BasicJsonType::string_t(utf8.constData(),
                static_cast<std::size_t>(utf8.size()));
        }

        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &json, QString &value)
        {
            const auto &string = json.template get_ref<
                const typename BasicJsonType::string_t &>();
            value = QString::fromUtf8(string.data(),
                static_cast<qsizetype>(string.size()));
        }
    };
} // namespace nlohmann

using JsonValue = nlohmann::ordered_json;
using JsonObject = JsonValue::object_t;
using JsonArray = JsonValue::array_t;

const JsonValue *findJsonValue(const JsonObject &object,
    const std::string &key);

template <typename T>
T jsonValue(const JsonObject &object, const std::string &key,
    const T &defaultValue = {})
{
    const auto value = findJsonValue(object, key);
    if (!value)
        return defaultValue;
    try {
        return value->get<T>();
    } catch (const nlohmann::json::exception &) {
        return defaultValue;
    }
}

const JsonObject *jsonObject(const JsonValue &value);
const JsonArray *jsonArray(const JsonValue &value);
const JsonArray *jsonArray(const JsonObject &object, const std::string &key);
std::string jsonKey(const QString &value);
QString jsonString(const JsonValue &value);
JsonValue jsonFromVariant(const QVariant &value);
QVariant variantFromJson(const JsonValue &value);
JsonValue parseJson(const QString &text);
QString serializeJson(const JsonValue &json);
