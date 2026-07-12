find_package(DuckDB CONFIG REQUIRED)

add_library(bt_duckdb_client STATIC
  src/warehouse.cpp
)
target_include_directories(bt_duckdb_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_duckdb_client PUBLIC
  $<IF:$<TARGET_EXISTS:duckdb>,duckdb,duckdb_static>
)
target_compile_features(bt_duckdb_client PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_duckdb_client_tests tests/warehouse_tests.cpp)
  target_link_libraries(bt_duckdb_client_tests PRIVATE bt_duckdb_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_duckdb_client_tests)
endif()
