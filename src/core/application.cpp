#include "application.h"

Application::Application(datatypes::Version version)
    : version(version) {
}

const datatypes::Version& Application::getVersion() const {
    return version;
}

int Application::run(int argc, char* argv[]) {
    return 0;
}