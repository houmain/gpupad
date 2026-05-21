#pragma once

#include <QString>
#include <cstdint>
#include <array>

struct AdapterIdentity
{
    using UUID = std::array<uint8_t, 16>;
    using LUID = std::array<uint8_t, 8>;

    QString name;
    UUID deviceUUIDs[4];
    UUID driverUUID;
    LUID deviceLUID;

    auto operator<=>(const AdapterIdentity &) const = default;
};
