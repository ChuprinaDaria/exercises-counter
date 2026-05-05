#pragma once
#include <cmath>

namespace exco {

constexpr float kPi = 3.14159265358979323846f;

inline float distance_2d(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

inline float angle_between(float ax, float ay,
                           float bx, float by,
                           float cx, float cy) {
    float bax = ax - bx;
    float bay = ay - by;
    float bcx = cx - bx;
    float bcy = cy - by;
    float dot = bax * bcx + bay * bcy;
    float cross = bax * bcy - bay * bcx;
    float rad = std::atan2(std::abs(cross), dot);
    return rad * 180.0f / kPi;
}

inline float normalize(float value, float min_val, float max_val) {
    if (max_val <= min_val) return 0.0f;
    return (value - min_val) / (max_val - min_val);
}

} // namespace exco
