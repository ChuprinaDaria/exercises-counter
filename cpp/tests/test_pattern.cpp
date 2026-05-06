#include "doctest.h"
#include "exco/pattern.hpp"
#include <cmath>
#include <vector>

TEST_CASE("dtw_distance — identical sequences = 0") {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    float d = exco::dtw_distance(a, a);
    CHECK(d == doctest::Approx(0.0f));
}

TEST_CASE("dtw_distance — similar sequences close to 0") {
    std::vector<float> a = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    std::vector<float> b = {1.1f, 2.1f, 3.1f, 2.1f, 1.1f};
    float d = exco::dtw_distance(a, b);
    CHECK(d < 1.0f);
}

TEST_CASE("dtw_distance — different sequences have large distance") {
    std::vector<float> a = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> b = {10.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    float d = exco::dtw_distance(a, b);
    CHECK(d > 5.0f);
}

TEST_CASE("extract_cycle — extracts one period from periodic signal") {
    std::vector<float> signal;
    for (int c = 0; c < 3; ++c) {
        for (int i = 0; i < 5; ++i) signal.push_back(static_cast<float>(i));
        for (int i = 4; i >= 0; --i) signal.push_back(static_cast<float>(i));
    }
    auto cycle = exco::extract_cycle(signal, 10);
    REQUIRE(cycle.size() == 10);
    CHECK(cycle[0] == doctest::Approx(0.0f).epsilon(0.5));
    CHECK(cycle[4] == doctest::Approx(4.0f).epsilon(0.5));
}

TEST_CASE("Pattern serialization round-trip") {
    exco::Pattern p;
    p.id = 0;
    p.period_frames = 20;
    p.signature = {1.0f, 2.0f, 3.0f};
    p.dominant_joints = {11, 13, 15};

    auto bytes = exco::serialize_pattern(p);
    auto restored = exco::deserialize_pattern(bytes);
    CHECK(restored.period_frames == 20);
    CHECK(restored.signature.size() == 3);
    CHECK(restored.dominant_joints.size() == 3);
    CHECK(restored.signature[1] == doctest::Approx(2.0f));
    CHECK(restored.dominant_joints[2] == 15);
}

TEST_CASE("PatternMatcher — finds matching pattern") {
    exco::PatternMatcher matcher(2.0f);
    exco::Pattern p;
    p.id = 1;
    p.period_frames = 5;
    p.signature = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    p.dominant_joints = {0};
    matcher.add_pattern(p);

    std::vector<float> query = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    std::vector<int> joints = {0};
    auto match = matcher.find_match(query, joints);
    REQUIRE(match.has_value());
    CHECK(match->id == 1);
}

TEST_CASE("PatternMatcher — no match for different pattern") {
    exco::PatternMatcher matcher(2.0f);
    exco::Pattern p;
    p.id = 1;
    p.period_frames = 5;
    p.signature = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    p.dominant_joints = {0};
    matcher.add_pattern(p);

    std::vector<float> query = {10.0f, 0.0f, 10.0f, 0.0f, 10.0f};
    std::vector<int> joints = {0};
    auto match = matcher.find_match(query, joints);
    CHECK_FALSE(match.has_value());
}
