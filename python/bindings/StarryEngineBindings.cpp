#include <pybind11/pybind11.h>
#include "../../src/application/Application.hpp"

namespace py = pybind11;

void run_engine() {
    StarryEngine::Application app;
    app.run();
}

PYBIND11_MODULE(starryengine_py, m) {
    m.doc() = "StarryEngine Python bindings";
    m.def("run_engine", &run_engine, "Start the engine main loop");
}