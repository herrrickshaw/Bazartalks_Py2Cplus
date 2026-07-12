#include "bazaartalks/quant_core/ml_signal_engine.hpp"

#include "bazaartalks/linalg/regression.hpp"
#include "bazaartalks/stats/technical_indicators.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
bool finite(double v) { return std::isfinite(v); }

std::vector<double> replace_zero_with_nan(const std::vector<double>& x) {
  std::vector<double> out(x.size());
  for (std::size_t i = 0; i < x.size(); ++i) out[i] = x[i] == 0.0 ? kNaN : x[i];
  return out;
}

std::vector<double> divide(const std::vector<double>& a, const std::vector<double>& b) {
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    out[i] = (finite(a[i]) && finite(b[i]) && b[i] != 0.0) ? a[i] / b[i] : kNaN;
  }
  return out;
}
}  // namespace

FeatureMatrix compute_features(const OhlcSeries& ohlc) {
  using bazaartalks::stats::ewm_mean_no_adjust;
  using bazaartalks::stats::rolling_mean;
  using bazaartalks::stats::rolling_std;
  using bazaartalks::stats::rsi;

  std::size_t n = ohlc.close.size();
  const auto& close = ohlc.close;
  const auto& high = ohlc.high;
  const auto& low = ohlc.low;
  std::vector<double> vol = replace_zero_with_nan(ohlc.volume);

  std::vector<double> ret_1d(n, kNaN), ret_5d(n, kNaN), pct_change_1d(n, kNaN);
  for (std::size_t i = 1; i < n; ++i) {
    pct_change_1d[i] = close[i - 1] != 0.0 ? (close[i] - close[i - 1]) / close[i - 1] : kNaN;
    ret_1d[i] = pct_change_1d[i] * 100.0;
  }
  for (std::size_t i = 5; i < n; ++i) {
    ret_5d[i] = close[i - 5] != 0.0 ? (close[i] - close[i - 5]) / close[i - 5] * 100.0 : kNaN;
  }

  auto vol_20d_raw = rolling_std(pct_change_1d, 20);
  std::vector<double> vol_20d(n);
  for (std::size_t i = 0; i < n; ++i) vol_20d[i] = vol_20d_raw[i] * std::sqrt(252.0) * 100.0;

  auto rsi_14 = rsi(close, 14);

  auto ema12 = ewm_mean_no_adjust(close, 12);
  auto ema26 = ewm_mean_no_adjust(close, 26);
  std::vector<double> macd(n);
  for (std::size_t i = 0; i < n; ++i) macd[i] = ema12[i] - ema26[i];
  auto macd_signal = ewm_mean_no_adjust(macd, 9);

  auto bb_mid = rolling_mean(close, 20);
  auto bb_std = rolling_std(close, 20);
  std::vector<double> bb_upper(n), bb_lower(n), bb_pct(n);
  for (std::size_t i = 0; i < n; ++i) {
    bb_upper[i] = bb_mid[i] + 2.0 * bb_std[i];
    bb_lower[i] = bb_mid[i] - 2.0 * bb_std[i];
    double range = bb_upper[i] - bb_lower[i];
    bb_pct[i] = (finite(range) && range != 0.0) ? (close[i] - bb_lower[i]) / range : kNaN;
  }

  auto dma50 = rolling_mean(close, 50);
  auto dma200 = rolling_mean(close, 200);
  std::vector<double> dma50_gap(n), dma200_gap(n);
  for (std::size_t i = 0; i < n; ++i) {
    dma50_gap[i] = (finite(dma50[i]) && dma50[i] != 0.0) ? (close[i] - dma50[i]) / dma50[i] * 100.0 : kNaN;
    dma200_gap[i] =
        (finite(dma200[i]) && dma200[i] != 0.0) ? (close[i] - dma200[i]) / dma200[i] * 100.0 : kNaN;
  }

  auto vol_avg = rolling_mean(vol, 20);
  auto vol_avg_nz = replace_zero_with_nan(vol_avg);
  auto vol_ratio = divide(vol, vol_avg_nz);

  auto close_nz = replace_zero_with_nan(close);
  std::vector<double> hl_ratio(n);
  for (std::size_t i = 0; i < n; ++i) {
    hl_ratio[i] = finite(close_nz[i]) ? (high[i] - low[i]) / close_nz[i] : kNaN;
  }

  auto close_mean20 = rolling_mean(close, 20);
  auto close_std20 = replace_zero_with_nan(rolling_std(close, 20));
  std::vector<double> close_norm(n);
  for (std::size_t i = 0; i < n; ++i) {
    close_norm[i] = (finite(close_std20[i])) ? (close[i] - close_mean20[i]) / close_std20[i] : kNaN;
  }

  // Assemble, then dropna() -- keep only rows where all 12 features are finite.
  FeatureMatrix out;
  std::vector<Eigen::VectorXd> rows;
  for (std::size_t i = 0; i < n; ++i) {
    std::array<double, 12> row = {ret_1d[i],     ret_5d[i],  vol_20d[i],     rsi_14[i],
                                   macd[i],       macd_signal[i], bb_pct[i],  dma50_gap[i],
                                   dma200_gap[i], vol_ratio[i], hl_ratio[i], close_norm[i]};
    bool all_finite = std::all_of(row.begin(), row.end(), [](double v) { return std::isfinite(v); });
    if (!all_finite) continue;
    Eigen::VectorXd erow(12);
    for (int c = 0; c < 12; ++c) erow(c) = row[c];
    rows.push_back(erow);
    out.source_index.push_back(i);
  }
  out.values.resize(static_cast<Eigen::Index>(rows.size()), 12);
  for (std::size_t r = 0; r < rows.size(); ++r) out.values.row(static_cast<Eigen::Index>(r)) = rows[r];
  return out;
}

Eigen::MatrixXd z_score_normalize_window(const Eigen::MatrixXd& window) {
  Eigen::RowVectorXd mean = window.colwise().mean();
  Eigen::Index rows = window.rows();
  Eigen::MatrixXd centered = window.rowwise() - mean;
  Eigen::RowVectorXd std_pop(window.cols());
  for (Eigen::Index c = 0; c < window.cols(); ++c) {
    double var = centered.col(c).array().square().sum() / static_cast<double>(rows);
    std_pop(c) = std::sqrt(var);  // numpy's default ddof=0 (population std)
    if (std_pop(c) == 0.0) std_pop(c) = 1.0;  // Python's `std[std==0]=1`
  }
  return centered.array().rowwise() / std_pop.array();
}

Sequences make_sequences(const Eigen::MatrixXd& features, const Eigen::VectorXd& target,
                         int lookback, int predict_days) {
  Eigen::Index n = features.rows();
  Eigen::Index n_feat = features.cols();
  std::vector<Eigen::RowVectorXd> X_rows;
  std::vector<double> y_vals;

  for (Eigen::Index i = lookback; i < n - predict_days; ++i) {
    Eigen::MatrixXd window = features.block(i - lookback, 0, lookback, n_feat);
    Eigen::MatrixXd z_win = z_score_normalize_window(window);
    // flatten() in Python is row-major (C order): window[0,:], window[1,:], ...
    Eigen::RowVectorXd flat(lookback * n_feat);
    for (Eigen::Index r = 0; r < lookback; ++r) {
      flat.segment(r * n_feat, n_feat) = z_win.row(r);
    }
    X_rows.push_back(flat);
    y_vals.push_back(target(i + predict_days));
  }

  Sequences seq;
  seq.X.resize(static_cast<Eigen::Index>(X_rows.size()), lookback * n_feat);
  seq.y.resize(static_cast<Eigen::Index>(y_vals.size()));
  for (std::size_t r = 0; r < X_rows.size(); ++r) {
    seq.X.row(static_cast<Eigen::Index>(r)) = X_rows[r];
    seq.y(static_cast<Eigen::Index>(r)) = y_vals[r];
  }
  return seq;
}

MlSignal predict_ridge(const OhlcSeries& ohlc, double ridge_alpha) {
  MlSignal result;
  result.direction = "INSUFFICIENT_DATA";

  std::size_t n_bars = ohlc.close.size();
  if (n_bars < static_cast<std::size_t>(kMlMinRows)) return result;

  FeatureMatrix features = compute_features(ohlc);
  if (features.source_index.size() < static_cast<std::size_t>(kMlMinRows) - 50) return result;

  // target[i] = close.pct_change(PREDICT_DAYS).shift(-PREDICT_DAYS) * 100,
  // reindexed onto features' retained rows, then inner-joined (rows where
  // the shifted target is still defined, i.e. i+PREDICT_DAYS < n_bars).
  std::vector<double> target_at_source(n_bars, kNaN);
  for (std::size_t i = 0; i + kMlPredictDays < n_bars; ++i) {
    double base = ohlc.close[i];
    target_at_source[i] =
        base != 0.0 ? (ohlc.close[i + kMlPredictDays] - base) / base * 100.0 : kNaN;
  }

  std::vector<Eigen::Index> aligned_feat_rows;
  std::vector<double> aligned_target;
  for (std::size_t r = 0; r < features.source_index.size(); ++r) {
    double t = target_at_source[features.source_index[r]];
    if (finite(t)) {
      aligned_feat_rows.push_back(static_cast<Eigen::Index>(r));
      aligned_target.push_back(t);
    }
  }
  Eigen::Index n_aligned = static_cast<Eigen::Index>(aligned_feat_rows.size());
  if (n_aligned < kMlLookback + 50) return result;

  Eigen::MatrixXd feat_aligned(n_aligned, features.values.cols());
  Eigen::VectorXd tgt_aligned(n_aligned);
  for (Eigen::Index i = 0; i < n_aligned; ++i) {
    feat_aligned.row(i) = features.values.row(aligned_feat_rows[static_cast<std::size_t>(i)]);
    tgt_aligned(i) = aligned_target[static_cast<std::size_t>(i)];
  }

  Eigen::Index train_end = n_aligned - kMlPredictDays - 1;
  Eigen::Index train_start = std::max<Eigen::Index>(0, train_end - kMlTrainWindow);
  if (train_end <= train_start) return result;

  Eigen::MatrixXd train_feat = feat_aligned.block(train_start, 0, train_end - train_start,
                                                  feat_aligned.cols());
  Eigen::VectorXd train_tgt = tgt_aligned.segment(train_start, train_end - train_start);

  Sequences seq = make_sequences(train_feat, train_tgt, kMlLookback, kMlPredictDays);
  if (seq.X.rows() < 30) return result;

  auto fit = linalg::ridge_with_intercept(seq.X, seq.y, ridge_alpha);
  Eigen::VectorXd y_pred_train = seq.X * fit.beta;
  y_pred_train.array() += fit.intercept;

  double sq_err = (y_pred_train - seq.y).array().square().sum();
  double train_rmse = std::sqrt(sq_err / static_cast<double>(seq.y.size()));

  // Predict using the most recent LOOKBACK window of the FULL aligned
  // feature set (not just the training slice) -- matches Python's
  // `feat_df.iloc[-LOOKBACK:]` on the full `aligned` frame.
  Eigen::MatrixXd latest_window =
      feat_aligned.block(n_aligned - kMlLookback, 0, kMlLookback, feat_aligned.cols());
  Eigen::MatrixXd z_latest = z_score_normalize_window(latest_window);
  Eigen::RowVectorXd flat(kMlLookback * feat_aligned.cols());
  for (Eigen::Index r = 0; r < kMlLookback; ++r) {
    flat.segment(r * feat_aligned.cols(), feat_aligned.cols()) = z_latest.row(r);
  }
  double predicted_ret = flat.dot(fit.beta) + fit.intercept;

  // numpy ndarray.std() (what sklearn's predict() output is, a plain
  // array, not a pandas Series) defaults to ddof=0 (population std) --
  // NOT the ddof=1 sample std pandas Series.std() would use.
  double pred_mean = y_pred_train.mean();
  double pred_var = (y_pred_train.array() - pred_mean).square().sum() /
                     static_cast<double>(y_pred_train.size());
  double pred_std = std::sqrt(pred_var);
  if (pred_std <= 0.0) pred_std = 1.0;
  double confidence = std::min(1.0, std::abs(predicted_ret) / (2.0 * pred_std + 1e-9));

  std::string direction;
  if (predicted_ret >= kMlBullishThreshold) {
    direction = "BULLISH";
  } else if (predicted_ret <= kMlBearishThreshold) {
    direction = "BEARISH";
  } else {
    direction = "NEUTRAL";
  }

  result.direction = direction;
  result.predicted_ret_pct = std::round(predicted_ret * 1000.0) / 1000.0;
  result.confidence = std::round(confidence * 1000.0) / 1000.0;
  result.train_rmse = train_rmse;
  result.train_rows = static_cast<std::size_t>(seq.X.rows());
  return result;
}

}  // namespace bazaartalks::quant_core
