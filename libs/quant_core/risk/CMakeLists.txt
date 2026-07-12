add_library(bt_risk STATIC
  src/risk.cpp
)
target_include_directories(bt_risk PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_risk PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_risk_tests tests/risk_tests.cpp)
  target_link_libraries(bt_risk_tests PRIVATE bt_risk Catch2::Catch2WithMain)
  catch_discover_tests(bt_risk_tests)
endif()
