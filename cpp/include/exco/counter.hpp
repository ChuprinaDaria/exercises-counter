#pragma once

namespace exco {

class RepCounter {
public:
    RepCounter(float down_threshold, float up_threshold, int min_frames_in_state);
    void push(float value);
    int count() const;
    void reset();
private:
    enum class State { IDLE, DOWN, UP };
    float down_threshold_;
    float up_threshold_;
    int min_frames_;
    State state_;
    int frames_in_state_;
    int count_;
};

} // namespace exco
