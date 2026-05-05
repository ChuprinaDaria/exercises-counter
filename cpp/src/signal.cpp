#include "exco/signal.hpp"
#include <cmath>
#include <numeric>

namespace exco {

std::vector<float> smooth(const std::vector<float>& signal, int window) {
    if (window <= 1 || signal.empty()) return signal;
    std::vector<float> out(signal.size());
    int half = window / 2;
    for (int i = 0; i < static_cast<int>(signal.size()); ++i) {
        int start = std::max(0, i - half);
        int end = std::min(static_cast<int>(signal.size()), i + half + 1);
        float sum = 0.0f;
        for (int j = start; j < end; ++j) {
            sum += signal[static_cast<size_t>(j)];
        }
        out[static_cast<size_t>(i)] = sum / static_cast<float>(end - start);
    }
    return out;
}

std::vector<float> autocorrelate(const std::vector<float>& signal) {
    int n = static_cast<int>(signal.size());
    if (n == 0) return {};
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / static_cast<float>(n);
    float variance = 0.0f;
    for (float v : signal) {
        variance += (v - mean) * (v - mean);
    }
    if (variance < 1e-10f) {
        return std::vector<float>(signal.size(), 0.0f);
    }
    // Compute raw (biased) autocorrelation sums
    std::vector<float> result(signal.size());
    for (int lag = 0; lag < n; ++lag) {
        float sum = 0.0f;
        for (int i = 0; i < n - lag; ++i) {
            sum += (signal[static_cast<size_t>(i)] - mean) *
                   (signal[static_cast<size_t>(i + lag)] - mean);
        }
        result[static_cast<size_t>(lag)] = sum / static_cast<float>(n - lag);
    }
    // Normalize by R(0) so that R(0) = 1.0
    float r0 = result[0];
    if (r0 > 1e-10f) {
        for (auto& v : result) v /= r0;
    }
    return result;
}

std::optional<PeriodResult> find_period(const std::vector<float>& signal,
                                        int min_period, int max_period,
                                        float min_strength) {
    auto acorr = autocorrelate(signal);
    int n = static_cast<int>(acorr.size());
    if (n <= min_period) return std::nullopt;

    int limit = std::min(max_period + 1, n);

    // Find the first local peak above threshold in [min_period, max_period]
    for (int lag = min_period; lag < limit; ++lag) {
        float val = acorr[static_cast<size_t>(lag)];
        if (val < min_strength) continue;
        // Check it's a local maximum
        float prev = (lag > 0) ? acorr[static_cast<size_t>(lag - 1)] : 0.0f;
        float next = (lag + 1 < n) ? acorr[static_cast<size_t>(lag + 1)] : 0.0f;
        if (val >= prev && val >= next) {
            return PeriodResult{lag, val};
        }
    }

    return std::nullopt;
}

} // namespace exco
