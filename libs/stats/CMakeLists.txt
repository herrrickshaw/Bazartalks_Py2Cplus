add_library(bt_stats STATIC
  src/cross_sectional.cpp
)
target_include_directories(bt_stats PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_stats PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_stats_tests tests/cross_sectional_tests.cpp)
  target_link_libraries(bt_stats_tests PRIVATE bt_stats Catch2::Catch2WithMain)
  catch_discover_tests(bt_stats_tests)
endif()
