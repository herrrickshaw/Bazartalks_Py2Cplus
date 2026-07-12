add_library(bt_pit_backtest STATIC
  src/pit_backtest.cpp
)
target_include_directories(bt_pit_backtest PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_pit_backtest PUBLIC bt_calendar)
target_compile_features(bt_pit_backtest PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_pit_backtest_tests tests/pit_backtest_tests.cpp)
  target_link_libraries(bt_pit_backtest_tests PRIVATE bt_pit_backtest Catch2::Catch2WithMain)
  catch_discover_tests(bt_pit_backtest_tests)
endif()
