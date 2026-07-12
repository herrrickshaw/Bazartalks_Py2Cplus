// Golden tests generated via numpy, replicating factor_research.py's ols()
// normal-equation formula and the closed-form ridge used by
// ml_signal_engine.py.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/linalg/regression.hpp"

using bazaartalks::linalg::Matrix;
using bazaartalks::linalg::ols_with_stats;
using bazaartalks::linalg::pinv;
using bazaartalks::linalg::ridge;
using bazaartalks::linalg::ridge_with_intercept;
using bazaartalks::linalg::Vector;
using Catch::Approx;

namespace {
Matrix sample_X() {
  Matrix X(8, 2);
  X << 1, 2, 2, 1, 3, 4, 4, 3, 5, 6, 6, 5, 7, 8, 8, 9;
  return X;
}
Vector sample_y() {
  Vector y(8);
  y << 3.0, 4.0, 7.0, 8.0, 12.0, 13.0, 17.0, 19.5;
  return y;
}
}  // namespace

// numpy reference (see regression_tests golden generation script):
//   beta   = [-0.35784313725490274, 1.6176470588235303, 0.7401960784313735]
//   se     = [0.27940316464233067, 0.15262347595734616, 0.13276985895866836]
//   tvals  = [-1.2807411745424737, 10.59893996435788, 5.575030991497842]
//   r2     = 0.9974181405191804
TEST_CASE("ols_with_stats matches Python's factor_research.ols() normal-equation formula",
          "[linalg]") {
  auto result = ols_with_stats(sample_X(), sample_y());
  REQUIRE(result.beta.size() == 3);
  CHECK(result.beta(0) == Approx(-0.35784313725490274));
  CHECK(result.beta(1) == Approx(1.6176470588235303));
  CHECK(result.beta(2) == Approx(0.7401960784313735));

  CHECK(result.std_errors(0) == Approx(0.27940316464233067));
  CHECK(result.std_errors(1) == Approx(0.15262347595734616));
  CHECK(result.std_errors(2) == Approx(0.13276985895866836));

  CHECK(result.t_stats(0) == Approx(-1.2807411745424737));
  CHECK(result.t_stats(1) == Approx(10.59893996435788));
  CHECK(result.t_stats(2) == Approx(5.575030991497842));

  CHECK(result.r_squared == Approx(0.9974181405191804));
}

// numpy reference: np.linalg.solve(X.T@X + 1.0*I, X.T@y)
//   -> [1.441679626749607, 0.8400725764644932]
TEST_CASE("ridge matches closed-form (X^T X + alpha I)^-1 X^T y", "[linalg]") {
  Vector beta = ridge(sample_X(), sample_y(), /*alpha=*/1.0);
  REQUIRE(beta.size() == 2);
  CHECK(beta(0) == Approx(1.441679626749607));
  CHECK(beta(1) == Approx(0.8400725764644932));
}

// numpy reference: np.linalg.pinv([[1,2],[3,4],[5,6]])
//   -> [[-1.3333333333333324, -0.3333333333333325, 0.6666666666666657],
//       [1.0833333333333326, 0.33333333333333265, -0.41666666666666596]]
TEST_CASE("pinv matches numpy.linalg.pinv on a tall rectangular matrix", "[linalg]") {
  Matrix A(3, 2);
  A << 1, 2, 3, 4, 5, 6;
  Matrix P = pinv(A);
  REQUIRE(P.rows() == 2);
  REQUIRE(P.cols() == 3);
  CHECK(P(0, 0) == Approx(-1.3333333333333324));
  CHECK(P(0, 1) == Approx(-0.3333333333333325));
  CHECK(P(0, 2) == Approx(0.6666666666666657));
  CHECK(P(1, 0) == Approx(1.0833333333333326));
  CHECK(P(1, 1) == Approx(0.33333333333333265));
  CHECK(P(1, 2) == Approx(-0.41666666666666596));
}

// sklearn reference: Ridge(alpha=1.0, fit_intercept=True).fit(X, y)
//   coef_ = [1.3796153509659366, -1.9233212895426761, 0.4805774944890462]
//   intercept_ = 3.0396688554950035
TEST_CASE("ridge_with_intercept matches sklearn's Ridge(fit_intercept=True)", "[linalg]") {
  Matrix X(20, 3);
  X << 1.7886284734303186, 0.43650985051198943, 0.09649746807200862, -1.8634927033644908,
      -0.27738820251439905, -0.35475897926898675, -0.08274148148245977, -0.6270006768238473,
      -0.04381816897592824, -0.47721803035950267, -1.3138647533626822, 0.8846223804995846,
      0.8813180422075299, 1.7095730636529485, 0.05003364217686021, -0.40467741460089085,
      -0.5453599476195304, -1.5464773155829683, 0.9823674342581601, -1.1010676301114757,
      -1.1850465270201729, -0.20564989942254108, 1.4861483550745902, 0.23671626722691233,
      -1.0237851399264681, -0.7129932001120494, 0.6252449661628293, -0.1605133631869239,
      -0.76883635031923, -0.23003072227793905, 0.7450562664053708, 1.9761107831263025,
      -1.244123328955937, -0.6264169111883692, -0.8037660945765764, -2.4190831731786697,
      -0.9237920216957886, -1.0238757608428377, 1.1239779589574683, -0.1319142328009009,
      -1.6232854458352473, 0.6466754522701722, -0.35627075944674486, -1.7431410369534588,
      -0.5966496416841994, -0.5885943796882431, -0.8738822977622994, 0.02971381536101662,
      -2.248257767576606, -0.2677618648460562, 1.0131834418864942, 0.8527978409541492,
      1.1081874999349925, 1.1193906553188915, 1.4875431319925396, -1.118300684400365,
      0.845833407057182, -1.8608895289421137, -0.6028851040072183, -1.9144720434058142;
  Vector y(20);
  y << 4.96298649438213, 0.715531642291817, 4.088238579020215, 5.531678154576496,
      0.8603750060626566, 2.7255268020827423, 6.098457718376354, -0.26883295306318844,
      3.246725834543819, 4.375785140759154, -0.559191918583326, 2.548299080365346,
      4.208601783430798, 6.548999998933604, 5.701929948654753, 3.9473515736590485,
      0.7340451273300223, 2.647425759893268, 7.751256420054062, 0.5963661836728397;

  auto result = ridge_with_intercept(X, y, 1.0);
  REQUIRE(result.beta.size() == 3);
  CHECK(result.beta(0) == Approx(1.3796153509659366));
  CHECK(result.beta(1) == Approx(-1.9233212895426761));
  CHECK(result.beta(2) == Approx(0.4805774944890462));
  CHECK(result.intercept == Approx(3.0396688554950035));
}
