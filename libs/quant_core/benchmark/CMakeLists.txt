add_library(bt_benchmark STATIC
  src/benchmark.cpp
)
target_include_directories(bt_benchmark PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_benchmark PUBLIC bt_linalg)
target_compile_features(bt_benchmark PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_benchmark_tests tests/benchmark_tests.cpp)
  target_link_libraries(bt_benchmark_tests PRIVATE bt_benchmark Catch2::Catch2WithMain)
  catch_discover_tests(bt_benchmark_tests)
endif()
