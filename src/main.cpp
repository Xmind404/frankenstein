#include "./core/application.h"
#include "datatypes/version.h"

int main(int argc, char* argv[]) {
    Application app(datatypes::Version{1, 0, 1}, "2026-11-19");

    return app.run(argc, argv);
}
