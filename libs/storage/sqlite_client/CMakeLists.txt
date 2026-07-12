find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(bt_sqlite_client STATIC
  src/sqlite_db.cpp
)
target_include_directories(bt_sqlite_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_sqlite_client PUBLIC unofficial::sqlite3::sqlite3)
target_compile_features(bt_sqlite_client PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_sqlite_client_tests tests/sqlite_db_tests.cpp)
  target_link_libraries(bt_sqlite_client_tests PRIVATE bt_sqlite_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_sqlite_client_tests)
endif()
