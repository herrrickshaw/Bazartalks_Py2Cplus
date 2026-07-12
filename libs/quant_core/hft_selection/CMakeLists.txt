add_library(bt_hft_selection STATIC
  src/hft_selection.cpp
)
target_include_directories(bt_hft_selection PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_hft_selection PUBLIC bt_stats)
target_compile_features(bt_hft_selection PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_hft_selection_tests tests/hft_selection_tests.cpp)
  target_link_libraries(bt_hft_selection_tests PRIVATE bt_hft_selection Catch2::Catch2WithMain)
  catch_discover_tests(bt_hft_selection_tests)
endif()
