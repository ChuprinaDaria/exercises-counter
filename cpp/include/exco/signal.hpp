#pragma once
#include <vector>
#include <optional>

namespace exco {

struct PeriodResult {
    int period;
    float strength;
};

std::vector<float> smooth(const std::vector<float>& signal, int window);
std::vector<float> autocorrelate(const std::vector<float>& signal);
std::optional<PeriodResult> find_period(const std::vector<float>& signal,
                                        int min_period, int max_period);

} // namespace exco
