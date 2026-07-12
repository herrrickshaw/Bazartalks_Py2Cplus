add_library(bt_dvm_global STATIC
  src/dvm_global.cpp
)
target_include_directories(bt_dvm_global PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_dvm_global PUBLIC bt_stats)
target_compile_features(bt_dvm_global PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_dvm_global_tests tests/dvm_global_tests.cpp)
  target_link_libraries(bt_dvm_global_tests PRIVATE bt_dvm_global Catch2::Catch2WithMain)
  catch_discover_tests(bt_dvm_global_tests)
endif()
