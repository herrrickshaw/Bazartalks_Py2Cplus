// Integration tests against a real local Kafka broker (this environment
// runs Kafka via `brew services`, same as the original BazaarTalks repo's
// INFRA.md documents) -- these exercise the actual mandatory-publish
// contract from market_store.py's _emit_cdc(), not a mock.
#include <catch2/catch_test_macros.hpp>

#include "bazaartalks/streaming/kafka_producer.hpp"

using bazaartalks::streaming::CdcPublishError;
using bazaartalks::streaming::KafkaProducer;

TEST_CASE("publish_and_flush succeeds against a live local broker", "[kafka][integration]") {
  KafkaProducer producer("localhost:9092");
  REQUIRE_NOTHROW(producer.publish_and_flush(
      "bt_cpp_test_topic", "TESTKEY",
      R"({"op":"upsert","table":"ohlc_bars","ticker":"TESTKEY","n_bars":1,"latest":"2026-07-12"})"));
}

TEST_CASE("publish_and_flush raises CdcPublishError against an unreachable broker",
          "[kafka][integration]") {
  // Port 1 is not a Kafka broker on any reachable host -- produce()/flush()
  // should time out with the message still queued, matching _emit_cdc()'s
  // "not delivered within Ns" failure mode (mandatory, not swallowed).
  KafkaProducer producer("127.0.0.1:1");
  CHECK_THROWS_AS(
      producer.publish_and_flush("bt_cpp_test_topic", "TESTKEY", "{}",
                                  std::chrono::milliseconds(500)),
      CdcPublishError);
}
