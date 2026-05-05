#include "exco/counter.hpp"

namespace exco {

RepCounter::RepCounter(float down_threshold, float up_threshold, int min_frames_in_state)
    : down_threshold_(down_threshold)
    , up_threshold_(up_threshold)
    , min_frames_(min_frames_in_state)
    , state_(State::IDLE)
    , frames_in_state_(0)
    , count_(0) {}

void RepCounter::push(float value) {
    switch (state_) {
    case State::IDLE:
        if (value >= up_threshold_) {
            state_ = State::UP;
            frames_in_state_ = 1;
        } else if (value <= down_threshold_) {
            state_ = State::DOWN;
            frames_in_state_ = 1;
        }
        break;
    case State::UP:
        if (value >= up_threshold_) {
            ++frames_in_state_;
        } else if (value <= down_threshold_ && frames_in_state_ >= min_frames_) {
            state_ = State::DOWN;
            frames_in_state_ = 1;
            ++count_;
        } else if (value <= down_threshold_) {
            state_ = State::IDLE;
            frames_in_state_ = 0;
        }
        break;
    case State::DOWN:
        if (value <= down_threshold_) {
            ++frames_in_state_;
        } else if (value >= up_threshold_ && frames_in_state_ >= min_frames_) {
            state_ = State::UP;
            frames_in_state_ = 1;
        } else if (value >= up_threshold_) {
            state_ = State::IDLE;
            frames_in_state_ = 0;
        }
        break;
    }
}

int RepCounter::count() const { return count_; }

void RepCounter::reset() {
    state_ = State::IDLE;
    frames_in_state_ = 0;
    count_ = 0;
}

} // namespace exco
