add_library(bt_portfolio STATIC
  src/portfolio.cpp
)
target_include_directories(bt_portfolio PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_portfolio PUBLIC bt_linalg)
target_compile_features(bt_portfolio PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_portfolio_tests tests/portfolio_tests.cpp)
  target_link_libraries(bt_portfolio_tests PRIVATE bt_portfolio Catch2::Catch2WithMain)
  catch_discover_tests(bt_portfolio_tests)
endif()
