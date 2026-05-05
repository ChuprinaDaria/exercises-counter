#include "doctest.h"
#include "exco/geometry.hpp"
#include <cmath>

TEST_CASE("distance_2d") {
    CHECK(exco::distance_2d(0.0f, 0.0f, 3.0f, 4.0f) == doctest::Approx(5.0f));
    CHECK(exco::distance_2d(1.0f, 1.0f, 1.0f, 1.0f) == doctest::Approx(0.0f));
}

TEST_CASE("angle_between_three_points returns degrees") {
    // Straight line → 180 degrees
    float angle = exco::angle_between(0.0f, 0.0f, 1.0f, 0.0f, 2.0f, 0.0f);
    CHECK(angle == doctest::Approx(180.0f).epsilon(0.01));

    // Right angle
    float right = exco::angle_between(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    CHECK(right == doctest::Approx(90.0f).epsilon(0.01));
}

TEST_CASE("normalize_to_range") {
    CHECK(exco::normalize(5.0f, 0.0f, 10.0f) == doctest::Approx(0.5f));
    CHECK(exco::normalize(0.0f, 0.0f, 10.0f) == doctest::Approx(0.0f));
    CHECK(exco::normalize(10.0f, 0.0f, 10.0f) == doctest::Approx(1.0f));
}
