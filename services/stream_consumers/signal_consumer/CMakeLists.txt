find_package(nlohmann_json CONFIG REQUIRED)

add_library(bt_signal_consumer_lib STATIC
  src/signal_consumer.cpp
)
target_include_directories(bt_signal_consumer_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_signal_consumer_lib PUBLIC nlohmann_json::nlohmann_json)
target_compile_features(bt_signal_consumer_lib PUBLIC cxx_std_20)

add_executable(bt_signal_consumer src/main.cpp)
target_link_libraries(bt_signal_consumer PRIVATE bt_signal_consumer_lib bt_kafka_client)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_signal_consumer_tests tests/signal_consumer_tests.cpp)
  target_link_libraries(bt_signal_consumer_tests PRIVATE bt_signal_consumer_lib Catch2::Catch2WithMain)
  catch_discover_tests(bt_signal_consumer_tests)

  add_executable(bt_signal_consumer_integration_tests tests/signal_consumer_integration_tests.cpp)
  target_link_libraries(bt_signal_consumer_integration_tests PRIVATE
    bt_signal_consumer_lib bt_kafka_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_signal_consumer_integration_tests)
endif()
