add_library(bt_dvm_engine STATIC
  src/dvm_engine.cpp
)
target_include_directories(bt_dvm_engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_dvm_engine PUBLIC bt_stats)
target_compile_features(bt_dvm_engine PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_dvm_engine_tests tests/dvm_engine_tests.cpp)
  target_link_libraries(bt_dvm_engine_tests PRIVATE bt_dvm_engine Catch2::Catch2WithMain)
  catch_discover_tests(bt_dvm_engine_tests)
endif()
