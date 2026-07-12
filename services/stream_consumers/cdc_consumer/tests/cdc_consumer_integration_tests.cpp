// Validates the C++ consumer's wire-format compatibility against a real
// local Kafka broker: publish a CDC-shaped JSON message (the same shape
// market_store.py's _emit_cdc() / kafka_cdc_consumer.py's producer side
// publishes) via the already-ported KafkaProducer, then confirm the
// ported parse_cdc()/CdcWindowAggregator correctly parses and counts it
// when fed through the ported KafkaConsumer.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/services/cdc_consumer.hpp"
#include "bazaartalks/streaming/kafka_consumer.hpp"
#include "bazaartalks/streaming/kafka_producer.hpp"

using namespace bazaartalks::services::cdc_consumer;
using namespace bazaartalks::streaming;

TEST_CASE("cdc_consumer aggregates a real CDC event round-tripped through Kafka",
          "[kafka][integration]") {
  KafkaProducer producer("localhost:9092");
  producer.publish_and_flush("bt_cpp_cdc_integration_topic", "AAPL",
                             R"({"op":"upsert","table":"ohlc_bars","ticker":"AAPL","n_bars":7})");
  producer.publish_and_flush("bt_cpp_cdc_integration_topic", "MSFT",
                             R"({"op":"upsert","table":"ohlc_bars","ticker":"MSFT","n_bars":2})");

  KafkaConsumer consumer("localhost:9092", "bt_cpp_cdc_integration_group");
  consumer.subscribe({"bt_cpp_cdc_integration_topic"});

  CdcWindowAggregator agg(/*window_seconds=*/3600.0, /*now=*/0.0);
  int seen = 0;
  for (int i = 0; i < 30 && seen < 2; ++i) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (msg && !msg->has_error) {
      auto parsed = parse_cdc(msg->value);
      if (parsed && (parsed->ticker == "AAPL" || parsed->ticker == "MSFT")) {
        agg.add(parsed->ticker, parsed->n_bars);
        ++seen;
      }
    }
  }
  REQUIRE(seen == 2);

  auto rows = agg.flush();
  REQUIRE(rows.size() == 2);
  CHECK(rows[0].first == "AAPL");
  CHECK(rows[0].second == 7);
  CHECK(rows[1].first == "MSFT");
  CHECK(rows[1].second == 2);
  consumer.close();
}
