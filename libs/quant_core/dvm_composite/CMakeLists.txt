add_library(bt_dvm_composite STATIC
  src/dvm_composite.cpp
)
target_include_directories(bt_dvm_composite PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_dvm_composite PUBLIC bt_stats)
target_compile_features(bt_dvm_composite PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_dvm_composite_tests tests/dvm_composite_tests.cpp)
  target_link_libraries(bt_dvm_composite_tests PRIVATE bt_dvm_composite Catch2::Catch2WithMain)
  catch_discover_tests(bt_dvm_composite_tests)
endif()
