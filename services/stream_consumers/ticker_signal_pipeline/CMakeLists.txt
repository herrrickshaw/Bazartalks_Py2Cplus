find_package(nlohmann_json CONFIG REQUIRED)

add_library(bt_ticker_signal_pipeline_lib STATIC
  src/ticker_signal_pipeline.cpp
)
target_include_directories(bt_ticker_signal_pipeline_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(bt_ticker_signal_pipeline_lib PUBLIC bt_stats nlohmann_json::nlohmann_json)
target_compile_features(bt_ticker_signal_pipeline_lib PUBLIC cxx_std_20)

add_executable(bt_stream_pipeline src/main.cpp)
target_link_libraries(bt_stream_pipeline PRIVATE
  bt_ticker_signal_pipeline_lib bt_kafka_client bt_cassandra_client)

if(BAZAARTALKS_BUILD_TESTS)
  add_executable(bt_ticker_signal_pipeline_tests tests/ticker_signal_pipeline_tests.cpp)
  target_link_libraries(bt_ticker_signal_pipeline_tests PRIVATE
    bt_ticker_signal_pipeline_lib Catch2::Catch2WithMain)
  catch_discover_tests(bt_ticker_signal_pipeline_tests)

  add_executable(bt_ticker_signal_pipeline_integration_tests
    tests/ticker_signal_pipeline_integration_tests.cpp)
  target_link_libraries(bt_ticker_signal_pipeline_integration_tests PRIVATE
    bt_ticker_signal_pipeline_lib bt_kafka_client Catch2::Catch2WithMain)
  catch_discover_tests(bt_ticker_signal_pipeline_integration_tests)
endif()
