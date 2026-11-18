#include "./core/application.h"

int main(int argc, char* argv[]) {
    Application app(datatypes::Version{1, 0, 1});

    return app.run(argc, argv);
}