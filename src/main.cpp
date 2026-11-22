#include "./core/application.h"
#include "datatypes/version.h"

#ifndef FRANKENSTEIN_VERSION_MAJOR
#define FRANKENSTEIN_VERSION_MAJOR 2
#define FRANKENSTEIN_VERSION_MINOR 0
#define FRANKENSTEIN_VERSION_PATCH 0
#define FRANKENSTEIN_VERSION_DATE "2026-11-22 01:30"
#endif

int main(int argc, char* argv[]) {
  Application app(
    datatypes::Version{
      FRANKENSTEIN_VERSION_MAJOR,
      FRANKENSTEIN_VERSION_MINOR,
      FRANKENSTEIN_VERSION_PATCH
    },
    FRANKENSTEIN_VERSION_DATE
  );

  return app.run(argc, argv);
}
