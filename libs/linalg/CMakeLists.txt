add_library(bt_linalg STATIC
  src/regression.cpp
)
target_include_directories(bt_linalg PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_linalg PUBLIC Eigen3::Eigen)
target_compile_features(bt_linalg PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_linalg_tests tests/regression_tests.cpp)
  target_link_libraries(bt_linalg_tests PRIVATE bt_linalg Catch2::Catch2WithMain)
  catch_discover_tests(bt_linalg_tests)
endif()
