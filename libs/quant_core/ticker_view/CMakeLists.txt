add_library(bt_ticker_view STATIC
  src/ticker_view.cpp
)
target_include_directories(bt_ticker_view PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_ticker_view PUBLIC bt_charts)
target_compile_features(bt_ticker_view PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_ticker_view_tests tests/ticker_view_tests.cpp)
  target_link_libraries(bt_ticker_view_tests PRIVATE bt_ticker_view Catch2::Catch2WithMain)
  catch_discover_tests(bt_ticker_view_tests)
endif()
