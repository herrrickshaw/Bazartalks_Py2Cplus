// Integration tests against a real local Kafka broker (same environment
// convention as kafka_producer_tests.cpp) -- round-trips a message
// through the already-ported KafkaProducer to prove KafkaConsumer's wire
// format matches, the same validation goal Phase 2's Kafka client tests
// established for producer/Python interop.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/streaming/kafka_consumer.hpp"
#include "bazaartalks/streaming/kafka_producer.hpp"

using namespace bazaartalks::streaming;

TEST_CASE("KafkaConsumer receives a message published by KafkaProducer", "[kafka][integration]") {
  KafkaProducer producer("localhost:9092");
  producer.publish_and_flush("bt_cpp_consumer_test_topic", "CONSUMERTESTKEY",
                             R"({"ticker":"AAPL","n_bars":3})");

  KafkaConsumer consumer("localhost:9092", "bt_cpp_consumer_test_group");
  consumer.subscribe({"bt_cpp_consumer_test_topic"});

  std::optional<ConsumedMessage> found;
  for (int i = 0; i < 20 && !found; ++i) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (msg && !msg->has_error && msg->key == "CONSUMERTESTKEY") {
      found = msg;
    }
  }
  REQUIRE(found.has_value());
  CHECK(found->value == R"({"ticker":"AAPL","n_bars":3})");
  CHECK(found->has_error == false);
  consumer.close();
}

TEST_CASE("KafkaConsumer::poll never delivers a real message on a topic nothing "
          "has published to",
          "[kafka][integration]") {
  // A genuinely nonexistent topic yields an UNKNOWN_TOPIC_OR_PART error
  // event (has_error=true), not a bare timeout (nullopt) -- both are
  // "no message" outcomes matching confluent_kafka's `msg is None` /
  // `msg.error()` truthy branches, which the three Python consumer
  // originals both treat identically (skip this poll iteration).
  KafkaConsumer consumer("localhost:9092", "bt_cpp_consumer_empty_test_group");
  consumer.subscribe({"bt_cpp_consumer_empty_test_topic_never_published"});
  auto msg = consumer.poll(std::chrono::milliseconds(500));
  CHECK((!msg.has_value() || msg->has_error));
  consumer.close();
}
