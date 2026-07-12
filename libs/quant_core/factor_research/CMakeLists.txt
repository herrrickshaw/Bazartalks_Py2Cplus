add_library(bt_factor_research STATIC
  src/factor_research.cpp
)
target_include_directories(bt_factor_research PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_factor_research PUBLIC bt_portfolio)
target_compile_features(bt_factor_research PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_factor_research_tests tests/factor_research_tests.cpp)
  target_link_libraries(bt_factor_research_tests PRIVATE bt_factor_research Catch2::Catch2WithMain)
  catch_discover_tests(bt_factor_research_tests)
endif()
