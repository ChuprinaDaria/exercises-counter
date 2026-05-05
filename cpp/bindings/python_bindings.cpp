#include <pybind11/pybind11.h>
namespace py = pybind11;

PYBIND11_MODULE(_exco_cpp, m) {
    m.doc() = "Exercise counter C++ core";
}
