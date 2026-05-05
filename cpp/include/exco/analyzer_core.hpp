#pragma once
#include "exco/signal.hpp"
#include "exco/pattern.hpp"
#include "exco/counter.hpp"
#include <vector>
#include <optional>
#include <unordered_map>

namespace exco {

struct Landmark {
    float x, y, z, visibility;
};

struct AnalysisEvent {
    int pattern_id;   // -1 = new pattern detected
    int count;
    int period_frames;
    std::vector<float> signature;
    std::vector<int> dominant_joints;
};

struct AnalyzerConfig {
    int window_frames = 90;        // ~3 sec at 30 fps
    int min_period = 10;           // shortest allowed cycle
    int max_period = 90;           // longest allowed cycle
    float period_strength = 0.4f;  // autocorrelation threshold
    float dtw_threshold = 2.0f;    // max DTW distance for match
    float counter_down = 0.3f;
    float counter_up = 0.7f;
    int counter_min_frames = 3;
    int smooth_window = 5;
    int num_joints = 33;
};

class AnalyzerCore {
public:
    explicit AnalyzerCore(AnalyzerConfig config = {});

    // Feed one frame of landmarks (33 landmarks per frame).
    // Returns event if a rep was counted or new pattern detected.
    std::optional<AnalysisEvent> push_frame(const std::vector<Landmark>& landmarks);

    // Load previously saved patterns (from DB on startup)
    void load_pattern(const Pattern& p);

    const AnalyzerConfig& config() const;

private:
    AnalyzerConfig config_;
    PatternMatcher matcher_;

    // Per-joint signal buffers
    std::vector<std::vector<float>> joint_signals_;  // [joint_id][frame]
    int frame_count_;

    // Per-pattern counters
    std::unordered_map<int, RepCounter> counters_;
    int next_pattern_id_;

    std::vector<int> find_dominant_joints(int top_n) const;
    std::vector<float> build_composite(const std::vector<int>& joints) const;
};

} // namespace exco
