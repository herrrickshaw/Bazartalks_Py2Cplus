# The DataStax C/C++ driver for Cassandra has no vcpkg port and ships no
# CMake config or pkg-config file -- installed via Homebrew
# (`brew install cassandra-cpp-driver`), a system dependency the migration
# plan calls out explicitly since it has no vcpkg-manifest equivalent.
find_path(CASSANDRA_INCLUDE_DIR cassandra.h
  HINTS /opt/homebrew/opt/cassandra-cpp-driver/include
        /usr/local/opt/cassandra-cpp-driver/include
)
find_library(CASSANDRA_LIBRARY NAMES cassandra
  HINTS /opt/homebrew/opt/cassandra-cpp-driver/lib
        /usr/local/opt/cassandra-cpp-driver/lib
)
if(NOT CASSANDRA_INCLUDE_DIR OR NOT CASSANDRA_LIBRARY)
  message(FATAL_ERROR
    "cassandra-cpp-driver not found. Install via: brew install cassandra-cpp-driver")
endif()

add_library(bt_cassandra_client STATIC
  src/market_store.cpp
)
target_include_directories(bt_cassandra_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CASSANDRA_INCLUDE_DIR}
)
target_link_libraries(bt_cassandra_client PUBLIC
  ${CASSANDRA_LIBRARY}
  date::date
  bt_kafka_client
)
target_compile_features(bt_cassandra_client PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_cassandra_client_tests tests/market_store_tests.cpp)
  target_link_libraries(bt_cassandra_client_tests PRIVATE bt_cassandra_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_cassandra_client_tests)
endif()
