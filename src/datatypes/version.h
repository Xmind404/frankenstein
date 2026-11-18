#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <tuple>

namespace datatypes {

struct Version {
    std::uint32_t major = 0;
    std::uint32_t minor = 0;
    std::uint32_t patch = 0;

    bool operator==(const Version& other) const {
        return std::tie(major, minor, patch) == std::tie(other.major, other.minor, other.patch);
    }

    bool operator!=(const Version& other) const {
        return !(*this == other);
    }

    bool operator<(const Version& other) const {
        return std::tie(major, minor, patch) < std::tie(other.major, other.minor, other.patch);
    }

    bool operator>(const Version& other) const {
        return other < *this;
    }

    bool operator<=(const Version& other) const {
        return !(other < *this);
    }

    bool operator>=(const Version& other) const {
        return !(*this < other);
    }

    [[nodiscard]] std::string to_string() const;
    friend std::ostream& operator<<(std::ostream& os, const Version& v);
};

}
