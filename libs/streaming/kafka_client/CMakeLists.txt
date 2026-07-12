find_package(RdKafka CONFIG REQUIRED)

add_library(bt_kafka_client STATIC
  src/kafka_producer.cpp
  src/kafka_consumer.cpp
)
target_include_directories(bt_kafka_client PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_kafka_client PUBLIC RdKafka::rdkafka++)
target_compile_features(bt_kafka_client PUBLIC cxx_std_20)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_kafka_client_tests
    tests/kafka_producer_tests.cpp
    tests/kafka_consumer_tests.cpp
  )
  target_link_libraries(bt_kafka_client_tests PRIVATE bt_kafka_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_kafka_client_tests)
endif()
