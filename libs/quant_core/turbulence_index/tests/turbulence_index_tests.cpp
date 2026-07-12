// This module is NEW code (Mahalanobis-distance turbulence index), not a
// port -- cross-checked against an independent numpy implementation of
// the same formula on a fixed synthetic 60-day x 4-asset return series
// (np.random.seed(200)), not "the actual Python output" (see this
// module's header for why there's no prior Python original to match).
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/quant_core/turbulence_index.hpp"

using namespace bazaartalks::quant_core;
using Catch::Approx;

namespace {
Eigen::MatrixXd sample_returns() {
  std::vector<double> flat = {
      -0.0140094825, 0.0196095313,  0.0076187915,  -0.0019773829, 0.0041146623,  0.0001705033,
      -0.0017134672, 0.0052725678,  -0.0064193937, 0.0084200593,  0.0012324913,  0.0135328603,
      0.0026348149,  0.0106734895,  0.0196171178,  -0.0047967163, 0.0189213516,  -0.0100723508,
      -0.0081291629, 0.0028763150,  -0.0110418177, 0.0126498404,  -0.0124375908, 0.0087272281,
      -0.0028215113, -0.0123142901, 0.0026853781,  0.0213347422,  -0.0019821806, 0.0031746040,
      0.0076251350,  -0.0060360768, 0.0021227964,  0.0226338050,  -0.0061553423, -0.0095900322,
      0.0239805122,  0.0065437586,  0.0069382910,  -0.0028175226, 0.0011986397,  0.0003528897,
      0.0176106875,  -0.0030644507, 0.0069311606,  -0.0019088278, -0.0250798096, -0.0190566295,
      0.0007570574,  0.0082328417,  0.0223609495,  0.0182857249,  0.0039017410,  -0.0016351859,
      0.0116773591,  0.0099702945,  0.0031300360,  -0.0181793528, -0.0034160120, -0.0110141763,
      0.0014743798,  0.0163203196,  -0.0043286855, 0.0024991277,  -0.0024465727, 0.0102823615,
      0.0001991958,  -0.0063652344, 0.0009974017,  0.0138764141,  -0.0018276790, 0.0060370177,
      -0.0147007408, 0.0124279792,  0.0057518578,  -0.0079640754, -0.0014689850, -0.0081301704,
      0.0103530909,  0.0001856567,  -0.0093463583, 0.0229698967,  -0.0000358755, 0.0060642577,
      -0.0195653453, -0.0063322017, 0.0051313872,  -0.0051406935, -0.0035408192, -0.0158806700,
      -0.0010125239, 0.0095744408,  0.0011739450,  0.0074049189,  -0.0001996847, 0.0079731893,
      -0.0025642407, 0.0023199109,  -0.0002213603, 0.0226796653,  0.0030845440,  -0.0068603076,
      0.0036125086,  -0.0006879521, 0.0082047831,  -0.0109883292, -0.0022289366, -0.0007485667,
      0.0067675835,  0.0120805552,  0.0040786937,  0.0057423795,  -0.0083974969, 0.0078690722,
      0.0166904563,  0.0026215972,  0.0008181006,  0.0125272375,  0.0034745439,  -0.0070361164,
      -0.0016495011, -0.0036238072, -0.0102824164, 0.0126248520,  -0.0030463776, -0.0041698477,
      -0.0061643198, -0.0019249065, -0.0103445550, -0.0002992433, 0.0085579400,  0.0000758641,
      -0.0064537307, -0.0008390765, -0.0190027664, -0.0058517242, 0.0081579565,  -0.0058968302,
      -0.0055884936, -0.0008055295, 0.0030737463,  -0.0181451829, 0.0054022774,  -0.0096929046,
      -0.0036132865, 0.0026408824,  -0.0038895725, -0.0062922120, 0.0244702707,  0.0136594368,
      -0.0030957085, -0.0008549052, 0.0043258300,  0.0047273453,  0.0143609443,  0.0105001438,
      0.0086490518,  -0.0038224009, 0.0131157625,  0.0108588743,  0.0013881590,  -0.0091813926,
      0.0058459462,  0.0071597247,  0.0054985347,  0.0037154286,  0.0064944615,  -0.0024594898,
      0.0127923449,  0.0006007315,  0.0077311791,  -0.0137595201, 0.0008292246,  -0.0081771840,
      0.0074673284,  -0.0006856477, -0.0169434015, -0.0151145563, -0.0059653444, 0.0007045260,
      0.0049375837,  0.0151492525,  -0.0106952533, 0.0057045974,  0.0080272929,  -0.0009479561,
      0.0011542526,  -0.0079437193, -0.0089861729, -0.0090172629, -0.0014478459, -0.0120115029,
      0.0029552970,  -0.0101997395, -0.0020665373, 0.0000332533,  0.0197273803,  -0.0171498398,
      0.0064205971,  0.0195577562,  0.0025256657,  -0.0065340284, -0.0063040333, 0.0090658198,
      0.0149643663,  -0.0144975251, 0.0080911579,  0.0108020250,  0.0071552946,  -0.0077629369,
      0.0080884889,  0.0312076344,  0.0088175166,  0.0143675220,  0.0101962171,  -0.0077258420,
      0.0131843893,  -0.0036817574, -0.0048322656, -0.0099264658, 0.0184246038,  -0.0016511014,
      0.0252053186,  -0.0147903765, -0.0121658088, -0.0011056362, 0.0202784597,  -0.0085237437,
      -0.0043716582, 0.0038805315,  0.0145173878,  0.0162023667,  0.0055168997,  0.0187304317,
      0.0031655566,  -0.0047612687, 0.0003077359,  0.0071502076,  -0.0069637607, 0.0045311593};
  Eigen::MatrixXd m(60, 4);
  for (int i = 0; i < 60; ++i)
    for (int j = 0; j < 4; ++j) m(i, j) = flat[static_cast<std::size_t>(i * 4 + j)];
  return m;
}
}  // namespace

// numpy cross-check: mu=returns.mean(0), Sigma=np.cov(returns.T),
// turbulence(mu + [0.001,-0.001,0.0005,0.0]) -> 0.02061848891891794
TEST_CASE("turbulence_index_from_history matches an independent numpy cross-check "
          "for a near-average (calm) return vector",
          "[turbulence_index]") {
  Eigen::MatrixXd returns = sample_returns();
  Eigen::VectorXd mu = returns.colwise().mean();
  Eigen::VectorXd y_normal(4);
  y_normal << mu(0) + 0.001, mu(1) - 0.001, mu(2) + 0.0005, mu(3) + 0.0;

  double t = turbulence_index_from_history(returns, y_normal);
  CHECK(t == Approx(0.02061848891891794).margin(1e-6));
}

// numpy cross-check: turbulence(mu + [-0.08,-0.06,-0.09,-0.07]) -> 238.12524831421769
// (a simulated multi-asset crash day -- every asset down hard together,
// exactly the co-movement the Mahalanobis distance is meant to flag)
TEST_CASE("turbulence_index_from_history is far higher for a simulated crash day",
          "[turbulence_index]") {
  Eigen::MatrixXd returns = sample_returns();
  Eigen::VectorXd mu = returns.colwise().mean();
  Eigen::VectorXd y_crash(4);
  y_crash << mu(0) - 0.08, mu(1) - 0.06, mu(2) - 0.09, mu(3) - 0.07;

  double t = turbulence_index_from_history(returns, y_crash);
  CHECK(t == Approx(238.12524831421769).margin(0.01));
}

// numpy cross-check: 99th percentile of the 60-day historical turbulence
// series -> 11.71894156279786; the crash day's turbulence (238.1) clears
// it, the calm day's (0.02) does not.
TEST_CASE("is_turbulent flags the crash day but not the calm day against the "
          "99th-percentile historical threshold",
          "[turbulence_index]") {
  Eigen::MatrixXd returns = sample_returns();
  Eigen::VectorXd mu = returns.colwise().mean();

  std::vector<double> historical_turbulence;
  for (Eigen::Index i = 0; i < returns.rows(); ++i) {
    historical_turbulence.push_back(turbulence_index_from_history(returns, returns.row(i)));
  }

  Eigen::VectorXd y_crash(4);
  y_crash << mu(0) - 0.08, mu(1) - 0.06, mu(2) - 0.09, mu(3) - 0.07;
  double t_crash = turbulence_index_from_history(returns, y_crash);

  Eigen::VectorXd y_calm(4);
  y_calm << mu(0) + 0.001, mu(1) - 0.001, mu(2) + 0.0005, mu(3) + 0.0;
  double t_calm = turbulence_index_from_history(returns, y_calm);

  CHECK(is_turbulent(t_crash, historical_turbulence, 0.99) == true);
  CHECK(is_turbulent(t_calm, historical_turbulence, 0.99) == false);
}
