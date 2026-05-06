#include "exco/analyzer_core.hpp"
#include "exco/geometry.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace exco {

AnalyzerCore::AnalyzerCore(AnalyzerConfig config)
    : config_(config)
    , matcher_(config.dtw_threshold)
    , joint_signals_(static_cast<size_t>(config.num_joints))
    , frame_count_(0)
    , next_pattern_id_(1)
    , active_pattern_id_(-1)
    , no_match_frames_(0) {}

void AnalyzerCore::load_pattern(const Pattern& p) {
    matcher_.add_pattern(p);
    counters_.emplace(p.id, RepCounter(config_.counter_down,
                                        config_.counter_up,
                                        config_.counter_min_frames));
    if (p.id >= next_pattern_id_) {
        next_pattern_id_ = p.id + 1;
    }
}

const AnalyzerConfig& AnalyzerCore::config() const {
    return config_;
}

std::vector<int> AnalyzerCore::find_dominant_joints(int top_n) const {
    std::vector<std::pair<float, int>> variances;
    for (int j : MAJOR_JOINTS) {
        const auto& sig = joint_signals_[static_cast<size_t>(j)];
        if (sig.size() < 2) {
            variances.push_back({0.0f, j});
            continue;
        }
        float mean = std::accumulate(sig.begin(), sig.end(), 0.0f) /
                     static_cast<float>(sig.size());
        float var = 0.0f;
        for (float v : sig) {
            var += (v - mean) * (v - mean);
        }
        var /= static_cast<float>(sig.size());
        variances.push_back({var, j});
    }
    std::sort(variances.begin(), variances.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<int> result;
    for (int i = 0; i < std::min(top_n, static_cast<int>(variances.size())); ++i) {
        if (variances[static_cast<size_t>(i)].first > 1e-6f) {
            result.push_back(variances[static_cast<size_t>(i)].second);
        }
    }
    return result;
}

std::vector<float> AnalyzerCore::build_composite(const std::vector<int>& joints) const {
    if (joints.empty()) return {};
    const auto& first = joint_signals_[static_cast<size_t>(joints[0])];
    size_t len = first.size();
    std::vector<float> composite(len, 0.0f);
    for (int j : joints) {
        const auto& sig = joint_signals_[static_cast<size_t>(j)];
        for (size_t i = 0; i < len; ++i) {
            composite[i] += sig[i];
        }
    }
    float scale = 1.0f / static_cast<float>(joints.size());
    for (float& v : composite) v *= scale;
    return composite;
}

std::optional<AnalysisEvent> AnalyzerCore::push_frame(
        const std::vector<Landmark>& landmarks) {
    if (static_cast<int>(landmarks.size()) != config_.num_joints) {
        return std::nullopt;
    }

    // Append y-coordinate of each joint to signal buffers
    // Use previous value for low-visibility joints to avoid noise
    for (int j = 0; j < config_.num_joints; ++j) {
        auto& sig = joint_signals_[static_cast<size_t>(j)];
        float val = landmarks[static_cast<size_t>(j)].y;
        if (landmarks[static_cast<size_t>(j)].visibility < config_.min_visibility
            && !sig.empty()) {
            val = sig.back();  // hold last known position
        }
        sig.push_back(val);
        if (static_cast<int>(sig.size()) > config_.window_frames) {
            sig.pop_front();
        }
    }
    ++frame_count_;

    if (static_cast<int>(joint_signals_[0].size()) < config_.window_frames) {
        return std::nullopt;
    }

    auto dominant = find_dominant_joints(5);
    if (dominant.empty()) return std::nullopt;

    auto composite = build_composite(dominant);
    composite = smooth(composite, config_.smooth_window);

    auto period_result = find_period(composite, config_.min_period, config_.max_period,
                                      config_.period_strength);
    if (!period_result.has_value()) return std::nullopt;

    int period = period_result->period;

    auto cycle = extract_cycle(composite, period);
    if (cycle.empty()) return std::nullopt;

    // Normalize cycle to 0..1
    float cmin = *std::min_element(cycle.begin(), cycle.end());
    float cmax = *std::max_element(cycle.begin(), cycle.end());
    if (cmax - cmin > 1e-6f) {
        for (float& v : cycle) {
            v = (v - cmin) / (cmax - cmin);
        }
    }

    auto match = matcher_.find_match(cycle, dominant);

    // Normalize current composite value for counter
    float current = composite.back();
    float smin = *std::min_element(composite.begin(), composite.end());
    float smax = *std::max_element(composite.begin(), composite.end());
    float normalized = (smax - smin > 1e-6f)
        ? (current - smin) / (smax - smin)
        : 0.5f;

    if (match.has_value()) {
        no_match_frames_ = 0;

        // Detect pattern (re)start: different from active, or resumed after pause
        bool started = (match->id != active_pattern_id_);
        active_pattern_id_ = match->id;

        // Enrich pattern with new observation
        matcher_.update_pattern(match->id, cycle, dominant);

        auto it = counters_.find(match->id);
        if (it == counters_.end()) {
            counters_.emplace(match->id, RepCounter(config_.counter_down,
                                                     config_.counter_up,
                                                     config_.counter_min_frames));
            it = counters_.find(match->id);
        }
        int prev = it->second.count();
        it->second.push(normalized);

        if (started) {
            // Emit pattern_started event
            return AnalysisEvent{match->id, it->second.count(),
                                 period, {}, {}, true};
        }
        if (it->second.count() > prev) {
            return AnalysisEvent{match->id, it->second.count(),
                                 period, {}, {}, false};
        }
    } else {
        ++no_match_frames_;
        if (no_match_frames_ >= PAUSE_FRAMES) {
            active_pattern_id_ = -1;  // pattern stopped
        }

        Pattern new_p;
        new_p.id = next_pattern_id_++;
        new_p.period_frames = period;
        new_p.signature = cycle;
        new_p.dominant_joints = dominant;
        matcher_.add_pattern(new_p);
        counters_.emplace(new_p.id, RepCounter(config_.counter_down,
                                                config_.counter_up,
                                                config_.counter_min_frames));
        active_pattern_id_ = new_p.id;
        return AnalysisEvent{-1, 0, period, cycle, dominant, false};
    }

    return std::nullopt;
}

} // namespace exco
