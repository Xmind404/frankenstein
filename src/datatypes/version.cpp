#include "version.h"

#include <ostream>
#include <string>

namespace datatypes {

std::string Version::to_string() const {
  return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

std::ostream& operator<<(std::ostream& os, const Version& v) {
  return os << v.to_string();
}

}
