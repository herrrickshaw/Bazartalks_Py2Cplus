#pragma once
// Port of ml_signal_engine.py (AlQahtani et al. 2025 methodology): 12
// technical features from OHLCV, per-window z-score normalization,
// sliding-window sequence building, and a walk-forward Ridge-regression
// signal. Report 1's own assessment: this is "one of the easier ML
// pieces to port" -- Ridge is closed-form linear algebra (bazaartalks::
// linalg::ridge_with_intercept), not an iterative model needing a
// library dependency.
//
// Note the MACD here uses `ewm(adjust=False)`, NOT the `adjust=True`
// default dvm_engine.py's MACD (Phase 5) uses -- these are genuinely
// different formulas producing different numbers on the same input;
// bazaartalks::stats::ewm_mean_no_adjust (Phase 1) is the one this module
// needs.

#include <Eigen/Dense>

#include <cstddef>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

constexpr int kMlLookback = 60;
constexpr int kMlTrainWindow = 252;
constexpr int kMlPredictDays = 5;
constexpr int kMlMinRows = kMlLookback + kMlTrainWindow + kMlPredictDays + 10;  // 327
constexpr double kMlBullishThreshold = 0.5;
constexpr double kMlBearishThreshold = -0.5;

struct OhlcSeries {
  std::vector<double> close;
  std::vector<double> high;
  std::vector<double> low;
  std::vector<double> volume;
};

struct FeatureMatrix {
  // rows x 12 columns, in FEATURE_NAMES order: ret_1d, ret_5d, vol_20d,
  // rsi_14, macd, macd_signal, bb_pct, dma50_gap, dma200_gap, vol_ratio,
  // hl_ratio, close_norm.
  Eigen::MatrixXd values;
  // values.row(i) came from ohlc bar source_index[i] -- since compute_features()
  // performs the dropna() internally (matching Python's `.dropna()` at
  // the end of the function), this maps back to the original series.
  std::vector<std::size_t> source_index;
};

// Port of compute_features(): builds the 12 features per bar, then drops
// every row with any NaN feature (matching Python's trailing `.dropna()`)
// -- rows needing 200 bars of history (dma200_gap) or more are absent
// until bar 200, same as the Python original.
FeatureMatrix compute_features(const OhlcSeries& ohlc);

// Port of z_score_normalise(): per-COLUMN z-score over a 2D window
// (LOOKBACK x n_features). A zero-std column maps to std=1 (not
// NaN/skip), matching Python's `std[std==0]=1` -- this differs from
// z_rank()'s all-NaN degenerate case (quality_factor.hpp) and
// bazaartalks::stats::zscore()'s all-zero case; each module's own
// zero-variance convention is preserved, not unified.
Eigen::MatrixXd z_score_normalize_window(const Eigen::MatrixXd& window);

struct Sequences {
  Eigen::MatrixXd X;  // n_samples x (lookback * n_features), each row a flattened
                       // per-window-z-scored feature block
  Eigen::VectorXd y;   // n_samples, the PREDICT_DAYS-ahead target
};

// Port of MLSignalEngine._make_sequences(): for each t in
// [lookback, n - predict_days), flattens the per-window z-scored
// features[t-lookback : t] into one training row, paired with
// target[t + predict_days].
Sequences make_sequences(const Eigen::MatrixXd& features, const Eigen::VectorXd& target,
                         int lookback = kMlLookback, int predict_days = kMlPredictDays);

struct MlSignal {
  std::string direction;  // "BULLISH" | "NEUTRAL" | "BEARISH" | "INSUFFICIENT_DATA"
  double predicted_ret_pct = 0.0;
  double confidence = 0.0;
  double train_rmse = 0.0;
  std::size_t train_rows = 0;
};

// Port of MLSignalEngine.predict() for the Ridge path (model_type="ridge",
// the module's own default and the one Report 1 flags as portable --
// LinearRegression is just ridge_alpha=0, so the same function covers
// both). Walk-forward: trains ONLY on the most recent TRAIN_WINDOW rows
// before the prediction point (no lookahead), matching the Python
// original's "nonstationarity handling" design. Returns
// direction="INSUFFICIENT_DATA" for any of the same early-exit
// conditions Python checks (too few input bars, too few valid feature
// rows, too few training sequences).
MlSignal predict_ridge(const OhlcSeries& ohlc, double ridge_alpha = 1.0);

}  // namespace bazaartalks::quant_core
