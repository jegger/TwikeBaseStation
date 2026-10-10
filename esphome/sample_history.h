#pragma once
// Rolling history of averaged samples per fixed time slot (default 5 min,
// 24 h). Used for the current (A) chart. Plain C++ so it can be unit tested
// off-device (see tests/test_sample_history.cpp).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

class SampleHistory {
 public:
  static constexpr int64_t STEP_S = 300;
  static constexpr size_t MAX_SLOTS = 288;  // 24 h of 5 min slots

  // Add a sample taken at wall-clock time now_ts (unix seconds). The average of
  // all samples in a slot is recorded once the slot is over.
  // Returns true when the history changed.
  bool add(int64_t now_ts, float value) {
    if (std::isnan(value)) return false;
    const int64_t slot = now_ts / STEP_S;
    if (count_ == 0 || slot < slot_) {
      // First sample, or clock jumped back: start a fresh slot
      start(slot, value);
      return false;
    }
    if (slot == slot_) {
      sum_ += value;
      count_++;
      return false;
    }
    record(slot_, sum_ / float(count_));
    start(slot, value);
    return true;
  }

  // Load a previously published history (e.g. the retained MQTT message after
  // a reboot). Ignored once live data exists.
  void restore(int64_t end_ts, const std::vector<float> &values) {
    if (!values_.empty()) return;
    values_ = values;
    end_slot_ = end_ts / STEP_S;
    trim();
  }

  bool empty() const { return values_.empty(); }
  size_t size() const { return values_.size(); }
  float at(size_t i) const { return values_[i]; }
  int64_t end_ts() const { return end_slot_ * STEP_S; }

  // {"end":<unix ts where the last slot ends>,"step":300,"v":[v, v, null, ...]}
  std::string to_json(int decimals = 2) const {
    std::string s = "{\"end\":" + std::to_string(end_ts()) + ",\"step\":" + std::to_string(STEP_S) + ",\"v\":[";
    char buf[24];
    for (size_t i = 0; i < values_.size(); i++) {
      if (i) s += ',';
      if (std::isnan(values_[i])) {
        s += "null";
      } else {
        snprintf(buf, sizeof(buf), "%.*f", decimals, values_[i]);
        s += buf;
      }
    }
    s += "]}";
    return s;
  }

 private:
  void start(int64_t slot, float value) {
    slot_ = slot;
    sum_ = value;
    count_ = 1;
  }

  // Append the value for `slot`; slots skipped since the last one become NaN
  // (null in JSON). Slots at or before the last recorded one are ignored.
  void record(int64_t slot, float value) {
    if (!values_.empty()) {
      if (slot < end_slot_) return;
      const int64_t gap = slot - end_slot_;
      if (gap >= int64_t(MAX_SLOTS)) {
        values_.clear();
      } else {
        values_.insert(values_.end(), size_t(gap), NAN);
      }
    }
    values_.push_back(value);
    end_slot_ = slot + 1;
    trim();
  }

  void trim() {
    if (values_.size() > MAX_SLOTS) values_.erase(values_.begin(), values_.end() - MAX_SLOTS);
  }

  std::vector<float> values_;
  int64_t end_slot_ = 0;

  int64_t slot_ = 0;
  float sum_ = 0;
  uint32_t count_ = 0;
};

// Single instance shared by the ESPHome lambdas. (ESPHome emits globals before
// `includes`, so a `globals:` entry of this type would not compile.)
inline SampleHistory &current_history() {
  static SampleHistory instance;
  return instance;
}
