add_library(bt_meta_screen STATIC
  src/meta_screen.cpp
)
target_include_directories(bt_meta_screen PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_meta_screen PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_meta_screen_tests tests/meta_screen_tests.cpp)
  target_link_libraries(bt_meta_screen_tests PRIVATE bt_meta_screen Catch2::Catch2WithMain)
  catch_discover_tests(bt_meta_screen_tests)
endif()
