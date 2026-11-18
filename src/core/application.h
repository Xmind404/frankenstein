#pragma once

#include "../datatypes/version.h"

class Application {
  private:
    const datatypes::Version version;

  public:
    Application(datatypes::Version version);

    const datatypes::Version& getVersion() const;

    int run(int argc, char* argv[]);
};