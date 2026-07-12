add_library(bt_quality_factor STATIC
  src/quality_factor.cpp
)
target_include_directories(bt_quality_factor PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_quality_factor PUBLIC bt_linalg bt_stats)
target_compile_features(bt_quality_factor PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_quality_factor_tests tests/quality_factor_tests.cpp)
  target_link_libraries(bt_quality_factor_tests PRIVATE bt_quality_factor Catch2::Catch2WithMain)
  catch_discover_tests(bt_quality_factor_tests)
endif()
