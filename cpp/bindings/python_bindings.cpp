#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "exco/geometry.hpp"
#include "exco/signal.hpp"
#include "exco/pattern.hpp"
#include "exco/counter.hpp"
#include "exco/analyzer_core.hpp"

namespace py = pybind11;

PYBIND11_MODULE(_exco_cpp, m) {
    m.doc() = "Exercise counter C++ core";

    // geometry
    m.def("distance_2d", &exco::distance_2d,
          py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"));
    m.def("angle_between", &exco::angle_between,
          py::arg("ax"), py::arg("ay"),
          py::arg("bx"), py::arg("by"),
          py::arg("cx"), py::arg("cy"));
    m.def("normalize", &exco::normalize,
          py::arg("value"), py::arg("min_val"), py::arg("max_val"));

    // signal
    py::class_<exco::PeriodResult>(m, "PeriodResult")
        .def_readonly("period", &exco::PeriodResult::period)
        .def_readonly("strength", &exco::PeriodResult::strength);
    m.def("smooth", &exco::smooth, py::arg("signal"), py::arg("window"));
    m.def("autocorrelate", &exco::autocorrelate, py::arg("signal"));
    m.def("find_period", &exco::find_period,
          py::arg("signal"), py::arg("min_period"), py::arg("max_period"),
          py::arg("min_strength") = 0.3f);

    // pattern
    py::class_<exco::Pattern>(m, "Pattern")
        .def(py::init<>())
        .def_readwrite("id", &exco::Pattern::id)
        .def_readwrite("period_frames", &exco::Pattern::period_frames)
        .def_readwrite("signature", &exco::Pattern::signature)
        .def_readwrite("dominant_joints", &exco::Pattern::dominant_joints);
    m.def("dtw_distance", &exco::dtw_distance, py::arg("a"), py::arg("b"));
    m.def("extract_cycle", &exco::extract_cycle, py::arg("signal"), py::arg("period"));
    m.def("serialize_pattern", &exco::serialize_pattern, py::arg("p"));
    m.def("deserialize_pattern", &exco::deserialize_pattern, py::arg("data"));

    // counter
    py::class_<exco::RepCounter>(m, "RepCounter")
        .def(py::init<float, float, int>(),
             py::arg("down_threshold"), py::arg("up_threshold"), py::arg("min_frames_in_state"))
        .def("push", &exco::RepCounter::push, py::arg("value"))
        .def("count", &exco::RepCounter::count)
        .def("reset", &exco::RepCounter::reset);

    // landmark
    py::class_<exco::Landmark>(m, "Landmark")
        .def(py::init<>())
        .def_readwrite("x", &exco::Landmark::x)
        .def_readwrite("y", &exco::Landmark::y)
        .def_readwrite("z", &exco::Landmark::z)
        .def_readwrite("visibility", &exco::Landmark::visibility);

    // analysis event
    py::class_<exco::AnalysisEvent>(m, "AnalysisEvent")
        .def_readonly("pattern_id", &exco::AnalysisEvent::pattern_id)
        .def_readonly("count", &exco::AnalysisEvent::count)
        .def_readonly("period_frames", &exco::AnalysisEvent::period_frames)
        .def_readonly("signature", &exco::AnalysisEvent::signature)
        .def_readonly("dominant_joints", &exco::AnalysisEvent::dominant_joints)
        .def_readonly("pattern_started", &exco::AnalysisEvent::pattern_started);

    // config
    py::class_<exco::AnalyzerConfig>(m, "AnalyzerConfig")
        .def(py::init<>())
        .def_readwrite("window_frames", &exco::AnalyzerConfig::window_frames)
        .def_readwrite("min_period", &exco::AnalyzerConfig::min_period)
        .def_readwrite("max_period", &exco::AnalyzerConfig::max_period)
        .def_readwrite("period_strength", &exco::AnalyzerConfig::period_strength)
        .def_readwrite("dtw_threshold", &exco::AnalyzerConfig::dtw_threshold)
        .def_readwrite("counter_down", &exco::AnalyzerConfig::counter_down)
        .def_readwrite("counter_up", &exco::AnalyzerConfig::counter_up)
        .def_readwrite("counter_min_frames", &exco::AnalyzerConfig::counter_min_frames)
        .def_readwrite("smooth_window", &exco::AnalyzerConfig::smooth_window)
        .def_readwrite("num_joints", &exco::AnalyzerConfig::num_joints)
        .def_readwrite("new_pattern_delay", &exco::AnalyzerConfig::new_pattern_delay)
        .def_readwrite("pattern_switch_frames", &exco::AnalyzerConfig::pattern_switch_frames);

    // analyzer core
    py::class_<exco::AnalyzerCore>(m, "AnalyzerCore")
        .def(py::init<exco::AnalyzerConfig>(), py::arg("config") = exco::AnalyzerConfig{})
        .def("push_frame", &exco::AnalyzerCore::push_frame, py::arg("landmarks"))
        .def("load_pattern", &exco::AnalyzerCore::load_pattern, py::arg("p"))
        .def("config", &exco::AnalyzerCore::config);
}
