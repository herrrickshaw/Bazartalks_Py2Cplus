add_library(bt_unlisted_valuation STATIC
  src/unlisted_valuation.cpp
)
target_include_directories(bt_unlisted_valuation PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_unlisted_valuation PUBLIC bt_risk)
target_compile_features(bt_unlisted_valuation PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_unlisted_valuation_tests tests/unlisted_valuation_tests.cpp)
  target_link_libraries(bt_unlisted_valuation_tests PRIVATE bt_unlisted_valuation Catch2::Catch2WithMain)
  catch_discover_tests(bt_unlisted_valuation_tests)
endif()
