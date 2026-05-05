#include "doctest.h"
#include "exco/signal.hpp"
#include <cmath>
#include <vector>

TEST_CASE("smooth — moving average removes noise") {
    std::vector<float> noisy = {0.0f, 1.2f, 0.1f, 0.9f, 0.0f, 1.1f, -0.1f, 1.0f};
    auto smoothed = exco::smooth(noisy, 3);
    CHECK(smoothed.size() == noisy.size());
    float var_orig = 0.0f, var_smooth = 0.0f;
    float mean_o = 0.0f, mean_s = 0.0f;
    for (size_t i = 0; i < noisy.size(); ++i) {
        mean_o += noisy[i];
        mean_s += smoothed[i];
    }
    mean_o /= static_cast<float>(noisy.size());
    mean_s /= static_cast<float>(smoothed.size());
    for (size_t i = 0; i < noisy.size(); ++i) {
        var_orig += (noisy[i] - mean_o) * (noisy[i] - mean_o);
        var_smooth += (smoothed[i] - mean_s) * (smoothed[i] - mean_s);
    }
    CHECK(var_smooth < var_orig);
}

TEST_CASE("autocorrelate — detects periodicity") {
    std::vector<float> signal;
    for (int i = 0; i < 100; ++i) {
        signal.push_back(std::sin(2.0f * 3.14159265f * static_cast<float>(i) / 10.0f));
    }
    auto acorr = exco::autocorrelate(signal);
    CHECK(acorr.size() == signal.size());
    CHECK(acorr[0] >= acorr[1]);
    CHECK(acorr[10] > acorr[7]);
    CHECK(acorr[10] > acorr[13]);
}

TEST_CASE("find_period — returns correct period for sine") {
    std::vector<float> signal;
    for (int i = 0; i < 100; ++i) {
        signal.push_back(std::sin(2.0f * 3.14159265f * static_cast<float>(i) / 20.0f));
    }
    auto result = exco::find_period(signal, 5, 50);
    REQUIRE(result.has_value());
    CHECK(result->period == 20);
    CHECK(result->strength > 0.8f);
}

TEST_CASE("find_period — returns nullopt for flat signal") {
    std::vector<float> flat(100, 1.0f);
    auto result = exco::find_period(flat, 5, 50);
    CHECK_FALSE(result.has_value());
}
