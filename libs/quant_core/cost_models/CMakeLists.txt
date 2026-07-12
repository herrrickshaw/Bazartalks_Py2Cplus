add_library(bt_cost_models STATIC
  src/net_of_cost.cpp
)
target_include_directories(bt_cost_models PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_cost_models PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_cost_models_tests tests/net_of_cost_tests.cpp)
  target_link_libraries(bt_cost_models_tests PRIVATE bt_cost_models Catch2::Catch2WithMain)
  catch_discover_tests(bt_cost_models_tests)
endif()
