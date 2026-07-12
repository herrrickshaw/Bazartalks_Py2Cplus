add_library(bt_momentum_breakout STATIC
  src/momentum_breakout.cpp
)
target_include_directories(bt_momentum_breakout PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_momentum_breakout PUBLIC bt_stats)
target_compile_features(bt_momentum_breakout PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_momentum_breakout_tests tests/momentum_breakout_tests.cpp)
  target_link_libraries(bt_momentum_breakout_tests PRIVATE bt_momentum_breakout Catch2::Catch2WithMain)
  catch_discover_tests(bt_momentum_breakout_tests)
endif()
