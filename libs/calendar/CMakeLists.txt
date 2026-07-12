add_library(bt_calendar STATIC
  src/trading_calendar.cpp
)
target_include_directories(bt_calendar PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_calendar PUBLIC date::date)
target_compile_features(bt_calendar PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_calendar_tests tests/trading_calendar_tests.cpp)
  target_link_libraries(bt_calendar_tests PRIVATE bt_calendar Catch2::Catch2WithMain)
  catch_discover_tests(bt_calendar_tests)
endif()
