add_library(bt_turbulence_index STATIC
  src/turbulence_index.cpp
)
target_include_directories(bt_turbulence_index PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_turbulence_index PUBLIC bt_linalg bt_risk)
target_compile_features(bt_turbulence_index PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_turbulence_index_tests tests/turbulence_index_tests.cpp)
  target_link_libraries(bt_turbulence_index_tests PRIVATE bt_turbulence_index Catch2::Catch2WithMain)
  catch_discover_tests(bt_turbulence_index_tests)
endif()
