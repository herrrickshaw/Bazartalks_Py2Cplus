#include "bazaartalks/quant_core/screen_viability.hpp"

#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

double round_n(double x, int decimals) {
  double scale = std::pow(10.0, decimals);
  return std::round(x * scale) / scale;
}
}  // namespace

ScreenSignals screens_for(const std::vector<double>& close, const std::vector<double>& high,
                          const std::vector<double>& low, const std::vector<double>& volume) {
  using namespace bazaartalks::stats;
  std::size_t n = close.size();

  std::vector<double> rsi_v = rsi(close, 14);
  std::vector<double> hi52 = rolling_max(close, 252, 120);
  std::vector<double> lo52 = rolling_min(close, 252, 120);
  std::vector<double> vol20 = rolling_mean(volume, 20);
  std::vector<double> dma50 = rolling_mean(close, 50);
  std::vector<double> dma200 = rolling_mean(close, 200);

  ScreenSignals out;
  out.rsi_oversold.resize(n);
  out.near_52w_high.resize(n);
  out.price_vol_breakout.resize(n);
  out.darvas_proximity.resize(n);
  out.golden_crossover.resize(n);

  for (std::size_t i = 0; i < n; ++i) {
    // pct_change() > 0: NaN at i==0 (no prior bar) compares False, same as
    // every other NaN comparison here -- IEEE 754 makes this automatic in
    // C++, no special-casing needed to match pandas.
    bool up = (i >= 1) && (close[i] / close[i - 1] - 1.0 > 0.0);
    bool gc = (i >= 1) && (dma50[i] > dma200[i]) && (dma50[i - 1] <= dma200[i - 1]);

    out.rsi_oversold[i] = rsi_v[i] < 30.0;
    out.near_52w_high[i] = close[i] >= 0.90 * hi52[i];
    out.price_vol_breakout[i] = (volume[i] >= 5.0 * vol20[i]) && up;
    out.darvas_proximity[i] = (close[i] >= 0.90 * hi52[i]) && (close[i] >= 1.10 * lo52[i]) &&
                              (close[i] > 10.0) && (volume[i] > 1e5);
    out.golden_crossover[i] = gc;
  }
  return out;
}

std::vector<ScreenEvalRow> eval_ticker(const std::vector<double>& close,
                                        const std::vector<double>& high,
                                        const std::vector<double>& low,
                                        const std::vector<double>& volume, int horizon,
                                        double clip, std::size_t min_bars) {
  std::size_t n = close.size();
  if (n < min_bars + static_cast<std::size_t>(horizon)) return {};

  // fwd[i] = pct_change(horizon).shift(-horizon): the realised forward
  // return from bar i to bar i+horizon, clipped to +/-clip%. This looks
  // "ahead" of i deliberately -- it's the backtest's OUTCOME measurement,
  // not a live tradable signal, so it isn't a lookahead bug.
  std::vector<double> fwd(n, kNaN);
  double sum = 0.0;
  int count = 0;
  for (std::size_t i = 0; i + static_cast<std::size_t>(horizon) < n; ++i) {
    double raw = (close[i + horizon] / close[i] - 1.0) * 100.0;
    double clipped = std::min(clip, std::max(-clip, raw));
    fwd[i] = clipped;
    sum += clipped;
    ++count;
  }
  double base = count > 0 ? sum / static_cast<double>(count) : kNaN;

  ScreenSignals sig = screens_for(close, high, low, volume);
  std::vector<std::pair<std::string, const std::vector<bool>*>> screens = {
      {"rsi_oversold", &sig.rsi_oversold},
      {"near_52w_high", &sig.near_52w_high},
      {"price_vol_breakout", &sig.price_vol_breakout},
      {"darvas_proximity", &sig.darvas_proximity},
      {"golden_crossover", &sig.golden_crossover},
  };

  std::vector<ScreenEvalRow> out;
  out.reserve(screens.size());
  for (const auto& [name, mask] : screens) {
    double sum_r = 0.0;
    int n_r = 0;
    int n_pos = 0;
    for (std::size_t i = 0; i < n; ++i) {
      if ((*mask)[i] && std::isfinite(fwd[i])) {
        sum_r += fwd[i];
        if (fwd[i] > 0.0) ++n_pos;
        ++n_r;
      }
    }
    ScreenEvalRow row;
    row.screen = name;
    row.baseline = round_n(base, 4);
    if (n_r == 0) {
      row.n_signals = 0;
      row.avg_fwd = kNaN;
      row.hit_pct = kNaN;
      row.edge = kNaN;
    } else {
      double avg = sum_r / static_cast<double>(n_r);
      double hit = static_cast<double>(n_pos) / static_cast<double>(n_r) * 100.0;
      row.n_signals = n_r;
      row.avg_fwd = round_n(avg, 4);
      row.hit_pct = round_n(hit, 1);
      row.edge = round_n(avg - base, 4);
    }
    out.push_back(std::move(row));
  }
  return out;
}

std::vector<MarketScreenSummary> aggregate_market_screen_summary(
    const std::vector<TickerScreenRow>& rows) {
  struct Acc {
    int n_tickers = 0;
    long long total_signals = 0;
    double sum_fwd = 0.0;
    double sum_hit = 0.0;
    double sum_edge = 0.0;
    int n_pos_edge = 0;
  };
  std::map<std::pair<std::string, std::string>, Acc> groups;
  for (const auto& r : rows) {
    if (r.n_signals <= 0) continue;  // `WHERE n_signals > 0`
    Acc& a = groups[{r.market, r.screen}];
    ++a.n_tickers;
    a.total_signals += r.n_signals;
    a.sum_fwd += r.avg_fwd;
    a.sum_hit += r.hit_pct;
    a.sum_edge += r.edge;
    if (r.edge > 0.0) ++a.n_pos_edge;
  }

  std::vector<MarketScreenSummary> out;
  out.reserve(groups.size());
  for (const auto& [key, a] : groups) {
    double n = static_cast<double>(a.n_tickers);
    double avg_fwd_raw = a.sum_fwd / n;
    double avg_hit_raw = a.sum_hit / n;
    double avg_edge_raw = a.sum_edge / n;

    MarketScreenSummary s;
    s.market = key.first;
    s.screen = key.second;
    s.n_tickers = a.n_tickers;
    s.total_signals = a.total_signals;
    s.avg_fwd5d = round_n(avg_fwd_raw, 4);
    s.avg_hit_pct = round_n(avg_hit_raw, 1);
    s.avg_edge = round_n(avg_edge_raw, 4);
    s.pct_tickers_pos_edge =
        round_n(100.0 * static_cast<double>(a.n_pos_edge) / n, 1);
    // `viable` compares the UNROUNDED AVG(edge)/AVG(hit_pct) -- the SQL
    // CASE expression re-evaluates the aggregate, it does not read back
    // the already-rounded avg_edge/avg_hit_pct output columns.
    s.viable = (avg_edge_raw > 0.0 && avg_hit_raw > 50.0) ? "YES" : "no";
    out.push_back(std::move(s));
  }

  // `ORDER BY market, avg_edge DESC` -- SQLite resolves an ORDER BY alias
  // against the SELECT-list's computed (rounded) output, not the raw
  // aggregate.
  std::stable_sort(out.begin(), out.end(), [](const MarketScreenSummary& a,
                                              const MarketScreenSummary& b) {
    if (a.market != b.market) return a.market < b.market;
    return a.avg_edge > b.avg_edge;
  });
  return out;
}

}  // namespace bazaartalks::quant_core
