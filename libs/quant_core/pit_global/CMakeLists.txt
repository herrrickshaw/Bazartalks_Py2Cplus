add_library(bt_pit_global STATIC
  src/pit_global.cpp
)
target_include_directories(bt_pit_global PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_pit_global PUBLIC bt_pit_backtest)
target_compile_features(bt_pit_global PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_pit_global_tests tests/pit_global_tests.cpp)
  target_link_libraries(bt_pit_global_tests PRIVATE bt_pit_global Catch2::Catch2WithMain)
  catch_discover_tests(bt_pit_global_tests)
endif()
