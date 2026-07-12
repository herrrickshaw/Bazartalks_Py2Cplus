add_library(bt_query_catalog STATIC
  src/query_catalog.cpp
)
target_include_directories(bt_query_catalog PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(bt_query_catalog PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_query_catalog_tests tests/query_catalog_tests.cpp)
  target_link_libraries(bt_query_catalog_tests PRIVATE bt_query_catalog Catch2::Catch2WithMain)
  catch_discover_tests(bt_query_catalog_tests)
endif()
