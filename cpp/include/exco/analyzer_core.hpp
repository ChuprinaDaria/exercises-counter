#pragma once
#include "exco/signal.hpp"
#include "exco/pattern.hpp"
#include "exco/counter.hpp"
#include <vector>
#include <deque>
#include <optional>
#include <unordered_map>

namespace exco {

struct Landmark {
    float x, y, z, visibility;
};

struct AnalysisEvent {
    int pattern_id;   // -1 = new pattern detected
    int count;        // 0 = pattern_started event
    int period_frames;
    std::vector<float> signature;
    std::vector<int> dominant_joints;
    bool pattern_started;  // true when known pattern resumes after pause
};

struct AnalyzerConfig {
    int window_frames = 60;        // ~2 sec at 30 fps
    int min_period = 10;           // shortest allowed cycle
    int max_period = 60;           // longest allowed cycle
    float period_strength = 0.3f;  // autocorrelation threshold
    float dtw_threshold = 0.8f;    // max DTW distance for match
    float min_visibility = 0.5f;   // ignore joints below this
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

    // Per-joint signal buffers (deque for O(1) pop_front)
    std::vector<std::deque<float>> joint_signals_;  // [joint_id][frame]
    int frame_count_;

    // Per-pattern counters
    std::unordered_map<int, RepCounter> counters_;
    int next_pattern_id_;

    // Active pattern tracking
    int active_pattern_id_;       // currently matched pattern (-1 = none)
    int no_match_frames_;         // frames since last match
    static constexpr int PAUSE_FRAMES = 30;  // ~1 sec gap = pattern stopped

    std::vector<int> find_dominant_joints(int top_n) const;

    // Major body joints only (exclude face, hands, feet)
    // 11=left shoulder, 12=right shoulder, 13=left elbow, 14=right elbow,
    // 15=left wrist, 16=right wrist, 23=left hip, 24=right hip,
    // 25=left knee, 26=right knee, 27=left ankle, 28=right ankle
    static constexpr int MAJOR_JOINTS[] = {11,12,13,14,15,16,23,24,25,26,27,28};
    std::vector<float> build_composite(const std::vector<int>& joints) const;
};

} // namespace exco
