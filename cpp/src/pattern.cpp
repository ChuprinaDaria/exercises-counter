#include "exco/pattern.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace exco {

float dtw_distance(const std::vector<float>& a, const std::vector<float>& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    if (n == 0 || m == 0) return 0.0f;

    std::vector<std::vector<float>> dp(
        static_cast<size_t>(n + 1),
        std::vector<float>(static_cast<size_t>(m + 1), 1e30f));
    dp[0][0] = 0.0f;

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            float cost = std::abs(a[static_cast<size_t>(i - 1)] -
                                  b[static_cast<size_t>(j - 1)]);
            dp[static_cast<size_t>(i)][static_cast<size_t>(j)] = cost +
                std::min({dp[static_cast<size_t>(i - 1)][static_cast<size_t>(j)],
                          dp[static_cast<size_t>(i)][static_cast<size_t>(j - 1)],
                          dp[static_cast<size_t>(i - 1)][static_cast<size_t>(j - 1)]});
        }
    }
    return dp[static_cast<size_t>(n)][static_cast<size_t>(m)] / static_cast<float>(std::max(n, m));
}

std::vector<float> extract_cycle(const std::vector<float>& signal, int period) {
    if (period <= 0 || signal.empty()) return {};
    int num_cycles = static_cast<int>(signal.size()) / period;
    if (num_cycles == 0) {
        return std::vector<float>(signal.begin(),
                                  signal.begin() + std::min(period, static_cast<int>(signal.size())));
    }
    std::vector<float> avg(static_cast<size_t>(period), 0.0f);
    for (int c = 0; c < num_cycles; ++c) {
        for (int i = 0; i < period; ++i) {
            avg[static_cast<size_t>(i)] +=
                signal[static_cast<size_t>(c * period + i)];
        }
    }
    for (int i = 0; i < period; ++i) {
        avg[static_cast<size_t>(i)] /= static_cast<float>(num_cycles);
    }
    return avg;
}

std::vector<uint8_t> serialize_pattern(const Pattern& p) {
    std::vector<uint8_t> data;
    auto push = [&](const void* ptr, size_t sz) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(ptr);
        data.insert(data.end(), bytes, bytes + sz);
    };
    int32_t period = p.period_frames;
    push(&period, sizeof(period));
    int32_t sig_len = static_cast<int32_t>(p.signature.size());
    push(&sig_len, sizeof(sig_len));
    push(p.signature.data(), static_cast<size_t>(sig_len) * sizeof(float));
    int32_t joints_len = static_cast<int32_t>(p.dominant_joints.size());
    push(&joints_len, sizeof(joints_len));
    push(p.dominant_joints.data(), static_cast<size_t>(joints_len) * sizeof(int));
    return data;
}

Pattern deserialize_pattern(const std::vector<uint8_t>& data) {
    Pattern p{};
    size_t offset = 0;
    auto pull = [&](void* dst, size_t sz) {
        std::memcpy(dst, data.data() + offset, sz);
        offset += sz;
    };
    int32_t period = 0;
    pull(&period, sizeof(period));
    p.period_frames = period;
    int32_t sig_len = 0;
    pull(&sig_len, sizeof(sig_len));
    p.signature.resize(static_cast<size_t>(sig_len));
    pull(p.signature.data(), static_cast<size_t>(sig_len) * sizeof(float));
    int32_t joints_len = 0;
    pull(&joints_len, sizeof(joints_len));
    p.dominant_joints.resize(static_cast<size_t>(joints_len));
    pull(p.dominant_joints.data(), static_cast<size_t>(joints_len) * sizeof(int));
    return p;
}

PatternMatcher::PatternMatcher(float dtw_threshold)
    : dtw_threshold_(dtw_threshold) {}

void PatternMatcher::add_pattern(const Pattern& p) {
    patterns_.push_back(p);
}

std::optional<Pattern> PatternMatcher::find_match(const std::vector<float>& cycle) const {
    float best_dist = dtw_threshold_;
    const Pattern* best = nullptr;
    for (const auto& p : patterns_) {
        float d = dtw_distance(cycle, p.signature);
        if (d < best_dist) {
            best_dist = d;
            best = &p;
        }
    }
    if (best != nullptr) return *best;
    return std::nullopt;
}

const std::vector<Pattern>& PatternMatcher::patterns() const {
    return patterns_;
}

} // namespace exco
