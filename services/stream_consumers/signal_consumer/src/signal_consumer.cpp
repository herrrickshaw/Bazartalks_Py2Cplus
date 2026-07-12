#include "bazaartalks/services/signal_consumer.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>

namespace bazaartalks::services::signal_consumer {

std::optional<std::string> parse_signal(const std::string& raw) {
  try {
    auto j = nlohmann::json::parse(raw);
    return j.value("signal", std::string("UNKNOWN"));
  } catch (...) {
    return std::nullopt;
  }
}

SignalWindowAggregator::SignalWindowAggregator(double window_seconds, double now)
    : window_start_(now), window_seconds_(window_seconds) {}

void SignalWindowAggregator::add(const std::string& signal) {
  auto it = counts_.find(signal);
  if (it == counts_.end()) {
    insertion_order_.push_back(signal);
    counts_[signal] = 1;
  } else {
    ++it->second;
  }
}

bool SignalWindowAggregator::should_flush(double now) const {
  return now - window_start_ >= window_seconds_;
}

std::vector<std::pair<std::string, int>> SignalWindowAggregator::flush() const {
  std::vector<std::pair<std::string, int>> out;
  out.reserve(insertion_order_.size());
  for (const auto& signal : insertion_order_) {
    out.emplace_back(signal, counts_.at(signal));
  }
  // std::stable_sort preserves insertion_order_'s relative order for
  // equal counts, matching Counter.most_common()'s tie-breaking exactly.
  std::stable_sort(out.begin(), out.end(),
                   [](const auto& a, const auto& b) { return a.second > b.second; });
  return out;
}

void SignalWindowAggregator::reset(double now) {
  insertion_order_.clear();
  counts_.clear();
  window_start_ = now;
}

}  // namespace bazaartalks::services::signal_consumer
