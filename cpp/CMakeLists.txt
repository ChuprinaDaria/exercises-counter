set(EXCO_SOURCES
    src/signal.cpp
    src/pattern.cpp
    src/counter.cpp
    src/analyzer_core.cpp
)

set(EXCO_HEADERS
    include/exco/geometry.hpp
    include/exco/signal.hpp
    include/exco/pattern.hpp
    include/exco/counter.hpp
    include/exco/analyzer_core.hpp
)

add_library(exco_core STATIC ${EXCO_SOURCES})
target_include_directories(exco_core PUBLIC include)
target_compile_features(exco_core PUBLIC cxx_std_17)

# pybind11 module
find_package(pybind11 CONFIG QUIET)
if(pybind11_FOUND)
    pybind11_add_module(_exco_cpp bindings/python_bindings.cpp)
    target_link_libraries(_exco_cpp PRIVATE exco_core)
    install(TARGETS _exco_cpp DESTINATION exco)
endif()

# doctest
option(EXCO_BUILD_TESTS "Build tests" ON)
if(EXCO_BUILD_TESTS)
    add_executable(exco_tests
        tests/test_main.cpp
        tests/test_geometry.cpp
        tests/test_signal.cpp
        tests/test_pattern.cpp
    )
    target_link_libraries(exco_tests PRIVATE exco_core)
    target_include_directories(exco_tests PRIVATE tests)
    enable_testing()
    add_test(NAME exco_tests COMMAND exco_tests)
endif()
