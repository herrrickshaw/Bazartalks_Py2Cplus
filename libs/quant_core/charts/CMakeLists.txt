add_library(bt_charts STATIC
  src/charts.cpp
)
target_include_directories(bt_charts PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_charts PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_charts_tests tests/charts_tests.cpp)
  target_link_libraries(bt_charts_tests PRIVATE bt_charts Catch2::Catch2WithMain)
  catch_discover_tests(bt_charts_tests)
endif()
