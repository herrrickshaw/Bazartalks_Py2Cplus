find_package(Drogon CONFIG REQUIRED)

add_executable(bt_serve_api src/main.cpp)
target_link_libraries(bt_serve_api PRIVATE
  Drogon::Drogon
  bt_query_catalog
  bt_charts
  bt_duckdb_client
)
target_compile_features(bt_serve_api PUBLIC cxx_std_20)
