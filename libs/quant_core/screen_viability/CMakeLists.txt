add_library(bt_screen_viability STATIC
  src/screen_viability.cpp
)
target_include_directories(bt_screen_viability PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_screen_viability PUBLIC bt_stats)
target_compile_features(bt_screen_viability PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_screen_viability_tests tests/screen_viability_tests.cpp)
  target_link_libraries(bt_screen_viability_tests PRIVATE bt_screen_viability Catch2::Catch2WithMain)
  catch_discover_tests(bt_screen_viability_tests)
endif()
