#include "bazaartalks/quant_core/dvm_composite.hpp"

#include "bazaartalks/stats/cross_sectional.hpp"
#include "bazaartalks/stats/technical_indicators.hpp"

#include <cmath>
#include <limits>
#include <unordered_map>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Same Python-builtin-min/max NaN-collapses-to-0 behavior as
// dvm_engine.py's momentum_score() (see that module's own comment for
// why) -- dvm_composite.py's momentum() uses the identical
// `min(100, max(0, x))` scalar pattern, not numpy's element-wise version.
double scalar_clip0_100(double x) {
  double maxed = (x > 0.0) ? x : 0.0;
  return (maxed < 100.0) ? maxed : 100.0;
}

double round1(double x) { return std::round(x * 10.0) / 10.0; }

// pandas `.rank(pct=True) * 100`: average-tie rank over the finite subset,
// divided by the finite count -- NaN stays NaN (an absent optional input
// never receives a rank).
std::vector<double> pct_rank_x100(const std::vector<double>& x) {
  std::vector<std::size_t> idx;
  std::vector<double> finite_vals;
  for (std::size_t i = 0; i < x.size(); ++i) {
    if (std::isfinite(x[i])) {
      idx.push_back(i);
      finite_vals.push_back(x[i]);
    }
  }
  std::vector<double> out(x.size(), kNaN);
  if (finite_vals.empty()) return out;
  auto ranks = bazaartalks::stats::rank_average(finite_vals);
  double n = static_cast<double>(finite_vals.size());
  for (std::size_t j = 0; j < idx.size(); ++j) out[idx[j]] = ranks[j] / n * 100.0;
  return out;
}

// `.fillna(column.mean())`: fill each NaN with the skipna mean of the
// column itself (computed once, over whatever entries are finite).
std::vector<double> fillna_with_mean(const std::vector<double>& x) {
  double sum = 0.0;
  int count = 0;
  for (double v : x) {
    if (std::isfinite(v)) {
      sum += v;
      ++count;
    }
  }
  double mean = count > 0 ? sum / static_cast<double>(count) : kNaN;
  std::vector<double> out(x.size());
  for (std::size_t i = 0; i < x.size(); ++i) out[i] = std::isfinite(x[i]) ? x[i] : mean;
  return out;
}

char g(double x) { return x >= 50.0 ? 'G' : 'B'; }

std::string label_for(const std::string& code) {
  static const std::unordered_map<std::string, std::string> kLabels = {
      {"GGG", "Strong Performer"},        {"GGB", "Value Under Radar"},
      {"GBG", "Expensive Durable Mover"},  {"GBB", "Expensive Quality"},
      {"BGG", "Cheap Turnaround Mover"},   {"BGB", "Deep Value / Watch"},
      {"BBG", "Momentum Trap"},            {"BBB", "Weak / Avoid"},
  };
  auto it = kLabels.find(code);
  return it != kLabels.end() ? it->second : "-";
}

}  // namespace

double dvm_composite_momentum(const std::vector<double>& close) {
  using namespace bazaartalks::stats;

  if (close.size() < 200) return kNaN;
  std::size_t last = close.size() - 1;

  std::vector<double> rsi_v = rsi(close, 14);
  MacdResult macd_r = macd(close, 12, 26, 9);
  double dma50 = rolling_mean(close, 50)[last];
  double dma200 = rolling_mean(close, 200)[last];
  double px = close[last];
  double hi52 = rolling_max(close, 252, 150)[last];  // dvm_global.py's min_periods form

  double rsi_last = rsi_v[last];
  double macd_hist_last = macd_r.histogram[last];

  double subs[4];
  subs[0] = scalar_clip0_100(rsi_last <= 70.0 ? rsi_last : 70.0 - (rsi_last - 70.0) * 2.0);
  subs[1] = macd_hist_last > 0.0 ? 100.0 : 25.0;
  subs[2] = (px > dma50 && dma50 > dma200) ? 100.0 : (px > dma200 ? 60.0 : 20.0);
  subs[3] = scalar_clip0_100(100.0 + (px / hi52 - 1.0) * 300.0);  // dvm_global.py's dist_52w form

  double sum = 0.0;
  for (double v : subs) sum += v;
  return sum / 4.0;
}

double dvm_composite_durability(std::optional<double> roe, std::optional<double> de,
                                std::optional<double> rev_growth,
                                std::optional<double> op_margin,
                                std::optional<double> earn_growth) {
  double sum = 0.0;
  int count = 0;
  if (roe.has_value()) {
    double v = roe.value();
    sum += (v > 20.0) ? 90.0 : (v > 15.0) ? 75.0 : (v > 10.0) ? 55.0 : (v > 0.0) ? 35.0 : 15.0;
    ++count;
  }
  if (de.has_value()) {
    double v = de.value();
    sum += (v < 0.5) ? 90.0 : (v < 1.0) ? 70.0 : (v < 2.0) ? 45.0 : 20.0;
    ++count;
  }
  if (rev_growth.has_value()) {
    double v = rev_growth.value();
    sum += (v > 15.0) ? 85.0 : (v > 5.0) ? 60.0 : (v > 0.0) ? 40.0 : 20.0;
    ++count;
  }
  if (op_margin.has_value()) {
    double v = op_margin.value();
    sum += (v > 20.0) ? 85.0 : (v > 10.0) ? 60.0 : (v > 0.0) ? 40.0 : 20.0;
    ++count;
  }
  if (earn_growth.has_value()) {
    sum += (earn_growth.value() > 0.0) ? 75.0 : 35.0;
    ++count;
  }
  return count > 0 ? sum / static_cast<double>(count) : kNaN;
}

std::vector<DvmCompositeRow> build_dvm_composite(const std::vector<DvmCompositeInput>& inputs) {
  struct Kept {
    const DvmCompositeInput* in;
    double M;
    double D;
    double ey;  // NaN if unavailable
    double pb;  // NaN if unavailable
  };
  std::vector<Kept> kept;
  for (const auto& in : inputs) {
    double M = dvm_composite_momentum(in.close);
    double D = dvm_composite_durability(in.roe, in.de, in.rev_growth, in.op_margin,
                                        in.earn_growth);
    if (std::isnan(M) || std::isnan(D)) continue;  // `if pd.isna(M) or pd.isna(D): continue`
    double ey = (in.pe.has_value() && in.pe.value() > 0.0) ? 1.0 / in.pe.value() : kNaN;
    double pb = in.pb.has_value() ? in.pb.value() : kNaN;
    kept.push_back(Kept{&in, M, D, ey, pb});
  }

  std::vector<double> ey_col(kept.size()), pb_col(kept.size());
  for (std::size_t i = 0; i < kept.size(); ++i) {
    ey_col[i] = kept[i].ey;
    pb_col[i] = kept[i].pb;
  }
  std::vector<double> ey_rank = pct_rank_x100(ey_col);
  std::vector<double> pb_rank_raw = pct_rank_x100(pb_col);
  std::vector<double> pb_rank(pb_rank_raw.size());
  for (std::size_t i = 0; i < pb_rank_raw.size(); ++i) {
    pb_rank[i] = std::isfinite(pb_rank_raw[i]) ? (100.0 - pb_rank_raw[i]) : kNaN;
  }
  std::vector<double> ey_rank_filled = fillna_with_mean(ey_rank);
  std::vector<double> pb_rank_filled = fillna_with_mean(pb_rank);

  std::vector<DvmCompositeRow> out;
  out.reserve(kept.size());
  for (std::size_t i = 0; i < kept.size(); ++i) {
    const auto& k = kept[i];
    DvmCompositeRow row;
    row.market = k.in->market;
    row.ticker = k.in->ticker;
    row.sector = k.in->sector;
    double v_raw = 0.6 * ey_rank_filled[i] + 0.4 * pb_rank_filled[i];
    row.V = round1(v_raw);
    row.M = round1(k.M);
    row.D = round1(k.D);
    row.code = std::string() + g(row.D) + g(row.V) + g(row.M);
    row.label = label_for(row.code);
    row.composite = round1((row.D + row.V + row.M) / 3.0);
    out.push_back(std::move(row));
  }
  return out;
}

}  // namespace bazaartalks::quant_core
