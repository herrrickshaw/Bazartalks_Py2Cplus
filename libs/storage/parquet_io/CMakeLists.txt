find_package(Arrow CONFIG REQUIRED)
find_package(Parquet CONFIG REQUIRED)

add_library(bt_parquet_io STATIC
  src/wide_frame.cpp
)
target_include_directories(bt_parquet_io PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_parquet_io PUBLIC
  "$<IF:$<BOOL:${ARROW_BUILD_STATIC}>,Arrow::arrow_static,Arrow::arrow_shared>"
  "$<IF:$<BOOL:${ARROW_BUILD_STATIC}>,Parquet::parquet_static,Parquet::parquet_shared>"
  Eigen3::Eigen
  date::date
)
target_compile_features(bt_parquet_io PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  find_package(DuckDB CONFIG REQUIRED)
  add_executable(bt_parquet_io_tests tests/wide_frame_tests.cpp)
  target_link_libraries(bt_parquet_io_tests PRIVATE
    bt_parquet_io
    Catch2::Catch2WithMain
    $<IF:$<TARGET_EXISTS:duckdb>,duckdb,duckdb_static>
  )
  catch_discover_tests(bt_parquet_io_tests)
endif()
