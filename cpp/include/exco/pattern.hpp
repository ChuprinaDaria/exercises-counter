#pragma once
#include <vector>
#include <optional>
#include <cstdint>

namespace exco {

struct Pattern {
    int id;
    int period_frames;
    std::vector<float> signature;
    std::vector<int> dominant_joints;
};

float dtw_distance(const std::vector<float>& a, const std::vector<float>& b);
std::vector<float> extract_cycle(const std::vector<float>& signal, int period);
std::vector<uint8_t> serialize_pattern(const Pattern& p);
Pattern deserialize_pattern(const std::vector<uint8_t>& data);

class PatternMatcher {
public:
    explicit PatternMatcher(float dtw_threshold);
    void add_pattern(const Pattern& p);
    std::optional<Pattern> find_match(const std::vector<float>& cycle) const;
    const std::vector<Pattern>& patterns() const;
private:
    float dtw_threshold_;
    std::vector<Pattern> patterns_;
};

} // namespace exco
