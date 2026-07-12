#include "bazaartalks/quant_core/quality_factor.hpp"

#include "bazaartalks/linalg/regression.hpp"
#include "bazaartalks/stats/cross_sectional.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <set>

namespace bazaartalks::quant_core {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
bool finite(double v) { return std::isfinite(v); }

// pandas Series.rank(): ranks only the non-NaN entries (average-tie
// method) among themselves, leaving NaN positions as NaN. Reuses
// bazaartalks::stats::rank_average for the tie-averaging math, applied
// only to the finite subset.
std::vector<double> pandas_rank(const std::vector<double>& values) {
  std::vector<std::size_t> finite_idx;
  std::vector<double> finite_vals;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (finite(values[i])) {
      finite_idx.push_back(i);
      finite_vals.push_back(values[i]);
    }
  }
  std::vector<double> out(values.size(), kNaN);
  auto ranks = bazaartalks::stats::rank_average(finite_vals);
  for (std::size_t j = 0; j < finite_idx.size(); ++j) out[finite_idx[j]] = ranks[j];
  return out;
}
}  // namespace

std::vector<double> z_rank(const std::vector<double>& values) {
  auto r = pandas_rank(values);
  // population std (ddof=0) over the ranked (non-NaN) entries only.
  std::vector<double> ranked_only;
  for (double v : r)
    if (finite(v)) ranked_only.push_back(v);

  double sd = bazaartalks::stats::population_stddev(ranked_only);
  if (sd == 0.0 || std::isnan(sd)) {
    return std::vector<double>(values.size(), kNaN);  // z_rank's own degenerate case: all-NaN
  }
  double mean = bazaartalks::stats::mean_skipnan(ranked_only);
  std::vector<double> out(values.size());
  for (std::size_t i = 0; i < values.size(); ++i) {
    out[i] = finite(r[i]) ? (r[i] - mean) / sd : kNaN;
  }
  return out;
}

std::vector<QualityScore> quality_score_batch(const std::vector<QualityInputs>& rows) {
  std::size_t n = rows.size();

  auto extract = [&](auto member) {
    std::vector<double> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = rows[i].*member;
    return v;
  };
  auto signed_zrank = [](const std::vector<double>& v, double sign) {
    auto z = z_rank(v);
    for (double& x : z) x *= sign;
    return z;
  };
  auto row_mean_skipna = [&](const std::vector<std::vector<double>>& cols) {
    std::vector<double> out(n, kNaN);
    for (std::size_t i = 0; i < n; ++i) {
      double sum = 0.0;
      int count = 0;
      for (const auto& col : cols) {
        if (finite(col[i])) {
          sum += col[i];
          ++count;
        }
      }
      out[i] = count > 0 ? sum / static_cast<double>(count) : kNaN;
    }
    return out;
  };

  // profitability: roe(+1), roa(+1), op_margin(+1)
  auto profitability = row_mean_skipna({
      signed_zrank(extract(&QualityInputs::roe), +1),
      signed_zrank(extract(&QualityInputs::roa), +1),
      signed_zrank(extract(&QualityInputs::op_margin), +1),
  });
  // growth: rev_growth(+1), earn_growth(+1)
  auto growth = row_mean_skipna({
      signed_zrank(extract(&QualityInputs::rev_growth), +1),
      signed_zrank(extract(&QualityInputs::earn_growth), +1),
  });
  // safety: de(-1), beta(-1), vol(-1)
  auto safety = row_mean_skipna({
      signed_zrank(extract(&QualityInputs::de), -1),
      signed_zrank(extract(&QualityInputs::beta), -1),
      signed_zrank(extract(&QualityInputs::vol), -1),
  });
  // payout: div_yield(+1)
  auto payout = row_mean_skipna({
      signed_zrank(extract(&QualityInputs::div_yield), +1),
  });

  auto quality_raw = row_mean_skipna({profitability, growth, safety, payout});
  auto quality = z_rank(quality_raw);
  auto quality_pct_rank = pandas_rank(quality);
  // .rank(pct=True) = rank / (count of non-NaN), matching pandas exactly.
  std::size_t n_ranked = 0;
  for (double v : quality)
    if (finite(v)) ++n_ranked;

  std::vector<QualityScore> out(n);
  for (std::size_t i = 0; i < n; ++i) {
    double qs = kNaN;
    if (finite(quality_pct_rank[i]) && n_ranked > 0) {
      qs = std::round(quality_pct_rank[i] / static_cast<double>(n_ranked) * 100.0 * 10.0) / 10.0;
    }
    out[i] = QualityScore{profitability[i], growth[i],  safety[i],
                          payout[i],        quality_raw[i], quality[i], qs};
  }
  return out;
}

std::vector<std::string> assign_deciles(const std::vector<double>& quality_scores, double top,
                                        double bot) {
  auto ranks = pandas_rank(quality_scores);
  std::size_t n_ranked = 0;
  for (double v : quality_scores)
    if (finite(v)) ++n_ranked;

  std::vector<std::string> out(quality_scores.size());
  for (std::size_t i = 0; i < quality_scores.size(); ++i) {
    if (!finite(ranks[i]) || n_ranked == 0) {
      out[i] = "mid";  // NaN pct-rank compares false to top/bot -> falls to the "mid" else-branch
      continue;
    }
    double pct = ranks[i] / static_cast<double>(n_ranked);
    out[i] = pct >= top ? "quality" : (pct <= bot ? "junk" : "mid");
  }
  return out;
}

std::vector<double> value_weight(const std::vector<double>& mktcap) {
  std::vector<double> w(mktcap.size());
  double sum = 0.0;
  for (std::size_t i = 0; i < mktcap.size(); ++i) {
    w[i] = std::max(0.0, mktcap[i]);
    sum += w[i];
  }
  if (sum > 0.0) {
    for (double& x : w) x /= sum;
  } else {
    std::fill(w.begin(), w.end(), 1.0 / static_cast<double>(mktcap.size()));
  }
  return w;
}

PricePremiumResult price_premium(const std::vector<double>& pb, const std::vector<double>& quality,
                                  const std::vector<double>& mktcap,
                                  const std::vector<std::string>& market) {
  std::vector<double> log_pb, q, log_size;
  std::vector<std::string> mkt;
  for (std::size_t i = 0; i < pb.size(); ++i) {
    if (!finite(pb[i]) || !finite(quality[i]) || !finite(mktcap[i])) continue;
    if (!(pb[i] > 0.0) || !(mktcap[i] > 0.0)) continue;
    log_pb.push_back(std::log(pb[i]));
    q.push_back(quality[i]);
    log_size.push_back(std::log(mktcap[i]));
    mkt.push_back(market[i]);
  }
  std::size_t n = log_pb.size();
  if (n < 30) return PricePremiumResult{n, std::nullopt, std::nullopt, std::nullopt, std::nullopt};

  // pd.get_dummies(market, drop_first=True): categories sorted
  // ALPHABETICALLY, first category dropped as the reference level --
  // the "named trap" this port must replicate exactly.
  std::set<std::string> uniq_set(mkt.begin(), mkt.end());
  std::vector<std::string> uniq(uniq_set.begin(), uniq_set.end());  // std::set already sorted
  std::vector<std::string> dummy_cols(uniq.begin() + (uniq.empty() ? 0 : 1), uniq.end());

  linalg::Matrix X(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(2 + dummy_cols.size()));
  for (std::size_t i = 0; i < n; ++i) {
    X(static_cast<Eigen::Index>(i), 0) = q[i];
    X(static_cast<Eigen::Index>(i), 1) = log_size[i];
    for (std::size_t j = 0; j < dummy_cols.size(); ++j) {
      X(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(2 + j)) =
          (mkt[i] == dummy_cols[j]) ? 1.0 : 0.0;
    }
  }
  linalg::Vector y(static_cast<Eigen::Index>(n));
  for (std::size_t i = 0; i < n; ++i) y(static_cast<Eigen::Index>(i)) = log_pb[i];

  auto result = linalg::ols_with_stats(X, y);
  double coef = result.beta(1);   // intercept=0, quality=1, log_size=2, dummies=3..
  double t = result.t_stats(1);
  double premium_pct = std::round((std::exp(coef) - 1.0) * 100.0 * 10.0) / 10.0;

  return PricePremiumResult{n, coef, t, premium_pct, result.r_squared};
}

std::vector<DriverCorrelation> driver_breakdown(const std::vector<double>& profitability,
                                                 const std::vector<double>& growth,
                                                 const std::vector<double>& safety,
                                                 const std::vector<double>& payout,
                                                 const std::vector<double>& pb) {
  struct Dim {
    const char* name;
    const std::vector<double>* values;
  };
  std::vector<Dim> dims = {
      {"profitability", &profitability}, {"growth", &growth}, {"safety", &safety}, {"payout", &payout}};

  std::vector<double> log_pb_filtered;
  std::vector<std::vector<double>> dim_filtered(dims.size());
  for (std::size_t i = 0; i < pb.size(); ++i) {
    if (!finite(pb[i]) || !(pb[i] > 0.0)) continue;
    log_pb_filtered.push_back(std::log(pb[i]));
    for (std::size_t d = 0; d < dims.size(); ++d) dim_filtered[d].push_back((*dims[d].values)[i]);
  }

  std::vector<DriverCorrelation> out;
  for (std::size_t d = 0; d < dims.size(); ++d) {
    double corr = bazaartalks::stats::pearson_corr(dim_filtered[d], log_pb_filtered);
    out.push_back(DriverCorrelation{dims[d].name, std::round(corr * 1000.0) / 1000.0});
  }
  // stable_sort: pandas' sort_values() is a stable sort, so ties keep
  // their original (DIMENSIONS dict insertion) order.
  std::stable_sort(out.begin(), out.end(),
                   [](const DriverCorrelation& a, const DriverCorrelation& b) {
                     return a.corr_with_log_mb > b.corr_with_log_mb;
                   });
  return out;
}

}  // namespace bazaartalks::quant_core
