#pragma once
// Port of risk.py -- pure functions over a decimal return series (0.01 =
// +1%), no pandas/numpy-specific idioms beyond numpy's quantile
// interpolation, which is replicated exactly (see quantile_linear).
// Report 1's own assessment: "best first candidate... every function is a
// pure, stateless numeric transform over a 1D array."

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bazaartalks::quant_core {

// numpy.quantile's default (method="linear") interpolation: sorts a copy
// of `values`, then linearly interpolates between the two nearest ranks at
// position q*(n-1). Used by hist_var()/cvar() -- getting this exact is
// what makes those two match Python bit-for-bit, not just approximately.
double quantile_linear(std::vector<double> values, double q);

// Cumulative wealth from decimal returns (product of (1+r), NOT prefixed
// with an initial 1.0 -- matches np.cumprod(1+r) exactly, so equity_curve
// has the same length as `returns`, with equity_curve[0] = 1+returns[0]).
std::vector<double> equity_curve(const std::vector<double>& returns);

double max_drawdown(const std::vector<double>& returns);
double hist_var(const std::vector<double>& returns, double alpha = 0.05);
double cvar(const std::vector<double>& returns, double alpha = 0.05);

// Sample stddev (ddof=1), NOT population (ddof=0) -- a different
// convention from libs/stats' zscore(), which uses ddof=0. Both are
// faithful ports of their respective Python originals; don't unify them.
double ann_vol(const std::vector<double>& returns, int periods = 252);
double ann_return(const std::vector<double>& returns, int periods = 252);
double sharpe(const std::vector<double>& returns, double rf = 0.0, int periods = 252);
double sortino(const std::vector<double>& returns, double rf = 0.0, int periods = 252);

struct RegimeFlag {
  std::string regime;  // "risk_on" | "caution" | "risk_off" | "unknown"
  std::optional<double> vol_ratio;
  std::optional<bool> in_drawdown;
};

// Port of regime_flag(): trailing-window vol vs full-sample vol, plus a
// >2%-off-peak drawdown check. Returns "unknown" (both optionals empty) if
// there isn't at least window+1 observations, matching Python's early
// return rather than computing a misleadingly short-window ratio.
RegimeFlag regime_flag(const std::vector<double>& returns, std::size_t window = 63,
                       int periods = 252);

}  // namespace bazaartalks::quant_core
