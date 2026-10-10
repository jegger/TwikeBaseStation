// Off-device tests for sample_history.h
//   c++ -std=c++17 -I.. test_sample_history.cpp -o /tmp/t && /tmp/t

#include "sample_history.h"

#include <cassert>
#include <cstdio>

static const int64_t T0 = 1791626400;  // aligned to a 5 min slot

static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

static void averages_samples_per_slot() {
  SampleHistory h;
  assert(!h.add(T0 + 0, 2.0f));
  assert(!h.add(T0 + 30, 4.0f));
  assert(!h.add(T0 + 60, 6.0f));
  assert(h.empty());                    // slot not over yet
  assert(h.add(T0 + 300, 1.0f));        // next slot starts: first one recorded
  assert(h.size() == 1 && near(h.at(0), 4.0f));
  assert(h.end_ts() == T0 + 300);
}

static void nan_samples_ignored() {
  SampleHistory h;
  h.add(T0, 2.0f);
  assert(!h.add(T0 + 30, NAN));
  h.add(T0 + 60, 4.0f);
  h.add(T0 + 300, 0.0f);
  assert(near(h.at(0), 3.0f));
}

static void gaps_between_recorded_slots() {
  SampleHistory h;
  h.add(T0, 1.0f);
  h.add(T0 + 300, 2.0f);                // slot 0 = 1
  h.add(T0 + 1200, 3.0f);               // slot 1 = 2
  h.add(T0 + 1500, 4.0f);               // slot 4 = 3, slots 2..3 NaN
  assert(h.size() == 5);
  assert(std::isnan(h.at(2)) && std::isnan(h.at(3)));
  assert(near(h.at(4), 3.0f));
  assert(h.end_ts() == T0 + 1500);
}

static void restore_then_gap() {
  SampleHistory h;
  h.restore(T0, {1.0f, NAN, 2.0f});
  assert(h.size() == 3 && h.end_ts() == T0);
  h.add(T0 + 600, 5.0f);
  h.add(T0 + 900, 0.0f);                // records slot T0+600, gap of 2 before it
  assert(h.size() == 6);
  assert(std::isnan(h.at(3)) && std::isnan(h.at(4)) && near(h.at(5), 5.0f));
  h.restore(T0, {9.0f});                // ignored once data exists
  assert(h.size() == 6);
}

static void restored_overlap_ignored() {
  SampleHistory h;
  h.restore(T0 + 600, {1.0f, 2.0f});
  h.add(T0 + 300, 7.0f);                // slot already in the restored data
  h.add(T0 + 600, 8.0f);
  assert(h.size() == 2 && near(h.at(1), 2.0f));
}

static void capped_at_24h() {
  SampleHistory h;
  for (int i = 0; i < 400; i++) h.add(T0 + i * 300, float(i));
  assert(h.size() == SampleHistory::MAX_SLOTS);
  assert(h.end_ts() == T0 + 399 * 300);
  assert(near(h.at(SampleHistory::MAX_SLOTS - 1), 398.0f));
}

static void json_format() {
  SampleHistory h;
  h.restore(T0, {0.054f, NAN, 7.8349f});
  const std::string expected =
      "{\"end\":" + std::to_string(T0) + ",\"step\":300,\"v\":[0.05,null,7.83]}";
  assert(h.to_json() == expected);
}

int main() {
  averages_samples_per_slot();
  nan_samples_ignored();
  gaps_between_recorded_slots();
  restore_then_gap();
  restored_overlap_ignored();
  capped_at_24h();
  json_format();
  printf("sample_history: all tests passed\n");
  return 0;
}
