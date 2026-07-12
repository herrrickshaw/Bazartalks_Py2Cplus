add_library(bt_darvas_volume STATIC
  src/darvas_volume.cpp
)
target_include_directories(bt_darvas_volume PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_darvas_volume PUBLIC bt_stats)
target_compile_features(bt_darvas_volume PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_darvas_volume_tests tests/darvas_volume_tests.cpp)
  target_link_libraries(bt_darvas_volume_tests PRIVATE bt_darvas_volume Catch2::Catch2WithMain)
  catch_discover_tests(bt_darvas_volume_tests)
endif()
