// Validates wire-format compatibility against a real local Kafka broker:
// publish scan.signals-shaped JSON via the already-ported KafkaProducer
// (the same shape stream_pipeline.py's consume() worker publishes), then
// confirm parse_signal()/SignalWindowAggregator correctly count it when
// fed through the ported KafkaConsumer.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/services/signal_consumer.hpp"
#include "bazaartalks/streaming/kafka_consumer.hpp"
#include "bazaartalks/streaming/kafka_producer.hpp"

using namespace bazaartalks::services::signal_consumer;
using namespace bazaartalks::streaming;

TEST_CASE("signal_consumer aggregates real scan.signals events round-tripped through Kafka",
          "[kafka][integration]") {
  KafkaProducer producer("localhost:9092");
  producer.publish_and_flush("bt_cpp_signal_integration_topic", "AAPL",
                             R"({"ticker":"AAPL","signal":"BREAKOUT","rsi":65.2,"close":190.5})");
  producer.publish_and_flush("bt_cpp_signal_integration_topic", "MSFT",
                             R"({"ticker":"MSFT","signal":"BREAKOUT","rsi":70.1,"close":410.0})");
  producer.publish_and_flush("bt_cpp_signal_integration_topic", "NVDA",
                             R"({"ticker":"NVDA","signal":"OVERSOLD","rsi":25.0,"close":120.0})");

  KafkaConsumer consumer("localhost:9092", "bt_cpp_signal_integration_group");
  consumer.subscribe({"bt_cpp_signal_integration_topic"});

  SignalWindowAggregator agg(/*window_seconds=*/3600.0, /*now=*/0.0);
  int seen = 0;
  for (int i = 0; i < 30 && seen < 3; ++i) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (msg && !msg->has_error) {
      auto parsed = parse_signal(msg->value);
      if (parsed) {
        agg.add(*parsed);
        ++seen;
      }
    }
  }
  REQUIRE(seen == 3);

  auto rows = agg.flush();
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].first == "BREAKOUT");
  CHECK(rows[0].second == 2);
  CHECK(rows[1].first == "OVERSOLD");
  CHECK(rows[1].second == 1);
  consumer.close();
}
