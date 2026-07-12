#include "bazaartalks/services/cdc_consumer.hpp"

#include <nlohmann/json.hpp>

namespace bazaartalks::services::cdc_consumer {

std::optional<CdcRecord> parse_cdc(const std::string& raw) {
  try {
    auto j = nlohmann::json::parse(raw);
    CdcRecord rec;
    // `rec["ticker"]` (a KeyError if absent, caught by the outer except)
    // -- ticker is assumed to always be a JSON string, unlike n_bars'
    // `.get("n_bars", 0)` default; a non-string ticker is a divergence
    // from Python (which would happily use whatever type), but not one
    // any real CDC producer emits.
    rec.ticker = j.at("ticker").get<std::string>();
    rec.n_bars = j.value("n_bars", static_cast<std::int64_t>(0));
    return rec;
  } catch (...) {
    return std::nullopt;
  }
}

CdcWindowAggregator::CdcWindowAggregator(double window_seconds, double now)
    : window_start_(now), window_seconds_(window_seconds) {}

void CdcWindowAggregator::add(const std::string& ticker, std::int64_t n_bars) {
  counts_[ticker] += n_bars;
}

bool CdcWindowAggregator::should_flush(double now) const {
  return now - window_start_ >= window_seconds_;
}

std::vector<std::pair<std::string, std::int64_t>> CdcWindowAggregator::flush() const {
  return std::vector<std::pair<std::string, std::int64_t>>(counts_.begin(), counts_.end());
}

void CdcWindowAggregator::reset(double now) {
  counts_.clear();
  window_start_ = now;
}

}  // namespace bazaartalks::services::cdc_consumer
