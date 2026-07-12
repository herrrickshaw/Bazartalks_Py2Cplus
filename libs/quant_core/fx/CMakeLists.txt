add_library(bt_fx STATIC
  src/fx.cpp
)
target_include_directories(bt_fx PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_fx PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(bt_fx PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_fx_tests tests/fx_tests.cpp)
  target_link_libraries(bt_fx_tests PRIVATE bt_fx Catch2::Catch2WithMain)
  catch_discover_tests(bt_fx_tests)
endif()
