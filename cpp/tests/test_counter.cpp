#include "doctest.h"
#include "exco/counter.hpp"
#include <cmath>

TEST_CASE("RepCounter — counts peaks in sine wave") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 20; ++i) {
            float t = static_cast<float>(i) / 19.0f;
            float val = 0.5f + 0.5f * std::sin(2.0f * 3.14159265f * t);
            counter.push(val);
        }
    }
    CHECK(counter.count() == 3);
}

TEST_CASE("RepCounter — flat signal = 0 counts") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    for (int i = 0; i < 100; ++i) {
        counter.push(0.5f);
    }
    CHECK(counter.count() == 0);
}

TEST_CASE("RepCounter — reset clears state") {
    exco::RepCounter counter(0.3f, 0.7f, 2);
    for (int i = 0; i < 20; ++i) {
        float val = static_cast<float>(i % 2);
        counter.push(val);
    }
    counter.reset();
    CHECK(counter.count() == 0);
}

TEST_CASE("RepCounter — min_frames_in_state filters jitter") {
    exco::RepCounter counter(0.3f, 0.7f, 5);
    counter.push(0.0f);
    counter.push(1.0f);
    counter.push(0.0f);
    counter.push(1.0f);
    CHECK(counter.count() == 0);
}
