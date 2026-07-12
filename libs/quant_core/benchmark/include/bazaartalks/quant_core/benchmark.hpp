#pragma once
// Port of benchmark.py's regression math ONLY -- carhart_alpha() (Carhart
// 4-factor alpha regression against real Ken French factor data) and
// factor_premia() (annualised mean/vol/Sharpe of each published factor).
// Per the migration plan: the Ken-French CSV-inside-zip download/parse,
// and quality_lq_returns()/validate_quality()'s data assembly (reading
// quality_factor.py's scored universe + local price parquets), are
// data-ingestion/orchestration, NOT ported here -- reuses
// bazaartalks::linalg::ols_with_stats, the same normal-equation OLS
// factor_research.py's ols() already is.

#include "bazaartalks/linalg/regression.hpp"

#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

struct FactorLoading {
  std::string factor;
  double coef;
  double t_stat;
};

struct CarhartResult {
  std::size_t n;
  std::optional<double> alpha_daily;
  std::optional<double> alpha_t;
  std::optional<double> alpha_monthly_pct;
  std::optional<bool> alpha_reliable;  // n >= 400 -- below that, a diagnostic, not a claim
  std::vector<FactorLoading> loadings;
  std::optional<double> r_squared;
};

// Regresses a portfolio's daily EXCESS return on the available factors
// (`factor_names`/`factor_returns`, e.g. Mkt-RF/SMB/HML/Mom for Carhart
// 4-factor). Rows where `port_excess` or ANY factor column is NaN are
// dropped first (matching `pd.concat([...]).dropna()`). Returns
// {n, nullopt...} below 30 valid rows, matching Python's early return.
// `alpha_reliable` is false below 400 rows -- the loadings remain
// interpretable either way, only the alpha itself needs a long/broad
// sample to trust (per the Python original's own comment).
CarhartResult carhart_alpha(const std::vector<double>& port_excess,
                            const std::vector<std::string>& factor_names,
                            const std::vector<std::vector<double>>& factor_returns);

struct FactorPremium {
  std::string factor;
  double ann_mean_pct;
  double ann_vol_pct;
  double sharpe;  // NaN if ann_vol_pct is 0
  std::size_t n_days;
};

// Per factor: annualised mean (*252), annualised vol (ddof=1 *sqrt(252)),
// and Sharpe. Each column's own NaN entries are dropped independently
// (matching `fac[c].dropna()`) -- unlike carhart_alpha(), there is no
// cross-column alignment requirement here.
std::vector<FactorPremium> factor_premia(const std::vector<std::string>& factor_names,
                                         const std::vector<std::vector<double>>& factor_series);

}  // namespace bazaartalks::quant_core
