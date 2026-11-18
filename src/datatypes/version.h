
#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace datatypes {

struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    auto operator<=>(const Version&) const = default;

    [[nodiscard]] std::string to_string() const;

    friend std::ostream& operator<<(std::ostream& os, const Version& version);
};

}