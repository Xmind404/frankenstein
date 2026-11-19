#pragma once

#include "../datatypes/version.h"
#include <string>

class Application {
  private:
    const datatypes::Version version;
    const std::string version_date;

  public:
    Application(datatypes::Version version, std::string version_date);

    const datatypes::Version& getVersion() const;

    int run(int argc, char* argv[]);
    void printHelp(const std::string& command = {}) const;
};
