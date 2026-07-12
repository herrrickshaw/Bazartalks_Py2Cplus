// Validates wire-format compatibility against a real local Kafka broker:
// publish ticker names to scan.tickers (the shape stream_pipeline.py's
// produce() emits: key==value==ticker string) via the already-ported
// KafkaProducer, confirm the consumer side receives them via the ported
// KafkaConsumer. Does not exercise the Cassandra-backed OHLC fetch (out
// of scope / requires a seeded local Cassandra instance) -- this proves
// the work-queue wiring, not the full signal computation.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/streaming/kafka_consumer.hpp"
#include "bazaartalks/streaming/kafka_producer.hpp"

using namespace bazaartalks::streaming;

TEST_CASE("ticker_signal_pipeline's work-queue round-trips ticker names through Kafka",
          "[kafka][integration]") {
  KafkaProducer producer("localhost:9092");
  producer.publish_and_flush("bt_cpp_ticker_queue_integration_topic", "AAPL", "AAPL");
  producer.publish_and_flush("bt_cpp_ticker_queue_integration_topic", "MSFT", "MSFT");

  KafkaConsumer consumer("localhost:9092", "bt_cpp_ticker_queue_integration_group");
  consumer.subscribe({"bt_cpp_ticker_queue_integration_topic"});

  int seen = 0;
  std::vector<std::string> received;
  for (int i = 0; i < 30 && seen < 2; ++i) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (msg && !msg->has_error && (msg->value == "AAPL" || msg->value == "MSFT")) {
      received.push_back(msg->value);
      ++seen;
    }
  }
  REQUIRE(seen == 2);
  consumer.close();
}
