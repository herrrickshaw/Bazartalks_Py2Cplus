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
