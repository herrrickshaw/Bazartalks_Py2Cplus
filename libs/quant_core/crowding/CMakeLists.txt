add_library(bt_crowding STATIC
  src/crowding.cpp
)
target_include_directories(bt_crowding PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_crowding PUBLIC bt_stats)
target_compile_features(bt_crowding PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_crowding_tests tests/crowding_tests.cpp)
  target_link_libraries(bt_crowding_tests PRIVATE bt_crowding Catch2::Catch2WithMain)
  catch_discover_tests(bt_crowding_tests)
endif()
