#include "./core/application.h"
#include "datatypes/version.h"

#ifndef FRANKENSTEIN_VERSION_MAJOR
#define FRANKENSTEIN_VERSION_MAJOR 1
#define FRANKENSTEIN_VERSION_MINOR 0
#define FRANKENSTEIN_VERSION_PATCH 2
#define FRANKENSTEIN_VERSION_DATE "unknown"
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
