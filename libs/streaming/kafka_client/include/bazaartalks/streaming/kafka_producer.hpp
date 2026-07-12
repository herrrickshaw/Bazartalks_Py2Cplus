#pragma once
// Kafka producer wrapper matching market_store.py's `_emit_cdc()` mandatory-
// publish semantics: CDC events are not best-effort. A failure to construct
// the producer, enqueue the message, or get a broker ack within the timeout
// raises CdcPublishError rather than being swallowed -- a dead broker (or no
// consumer running) must surface at write time, not silently drop the
// real-time event. See market_store.py's own docstring: "Cassandra and the
// Kafka CDC publish are MANDATORY, not optional fallbacks."

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace bazaartalks::streaming {

class CdcPublishError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class KafkaProducer {
 public:
  explicit KafkaProducer(const std::string& bootstrap_servers);
  ~KafkaProducer();

  KafkaProducer(const KafkaProducer&) = delete;
  KafkaProducer& operator=(const KafkaProducer&) = delete;

  // Publish `value` keyed by `key` to `topic`, then block (flush) until the
  // broker acks or `timeout` elapses. Port of _emit_cdc()'s three failure
  // modes, each raising CdcPublishError:
  //   1. produce() enqueue failure (e.g. queue full)
  //   2. flush() timeout with messages still queued ("not delivered within
  //      Ns (`remaining` still queued)")
  //   3. the delivery-report callback recorded an error for this message
  //      ("rejected by broker")
  // Calling flush() (not a bare poll(0)) matters for the same reason the
  // Python original calls producer.flush(timeout=10): a short-lived
  // process could otherwise exit before librdkafka has actually sent the
  // message, silently dropping it.
  void publish_and_flush(const std::string& topic, const std::string& key,
                          const std::string& value,
                          std::chrono::milliseconds timeout = std::chrono::seconds(10));

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace bazaartalks::streaming
