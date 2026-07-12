#pragma once
// Kafka consumer wrapper matching confluent_kafka's `Consumer` usage
// pattern shared by kafka_cdc_consumer.py, kafka_signal_consumer.py, and
// stream_pipeline.py's `consume()`: construct with bootstrap.servers +
// group.id + auto.offset.reset="earliest" (auto-commit left at its
// library default, true, since none of the three Python originals ever
// override `enable.auto.commit`), subscribe to one topic, then poll in a
// loop.
//
// confluent_kafka's `consumer.poll(timeout)` returns `None` for "nothing
// happened this poll" and a Message object otherwise, which the caller
// then checks `msg.error()` on (truthy for a partition-EOF/error event,
// falsy for a real message). librdkafka's C++ `KafkaConsumer::consume()`
// instead always returns a non-null Message* whose err() encodes all
// three cases (ERR__TIMED_OUT / another error/event code / ERR_NO_ERROR).
// `poll()` below translates back to the Python-shaped two-state result
// the three call sites actually branch on: std::nullopt for "no message"
// (ERR__TIMED_OUT, Python's msg is None), and a populated ConsumedMessage
// for anything else -- with `has_error` set for a non-NO_ERROR event,
// mirroring `msg.error()` truthy.

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace bazaartalks::streaming {

class KafkaConsumerError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct ConsumedMessage {
  std::string key;
  std::string value;
  bool has_error = false;
  std::string error_str;
};

class KafkaConsumer {
 public:
  // `group_id`/`auto_offset_reset` match every Python original's literal
  // config -- earliest, a fixed group id per consumer type.
  KafkaConsumer(const std::string& bootstrap_servers, const std::string& group_id,
               const std::string& auto_offset_reset = "earliest");
  ~KafkaConsumer();

  KafkaConsumer(const KafkaConsumer&) = delete;
  KafkaConsumer& operator=(const KafkaConsumer&) = delete;

  void subscribe(const std::vector<std::string>& topics);

  // std::nullopt == confluent_kafka's `msg is None` (no event within
  // timeout); a populated ConsumedMessage otherwise, with `has_error` set
  // for any non-NO_ERROR result (matching `msg.error()` truthy, which
  // includes benign per-partition EOF events, not just real errors).
  std::optional<ConsumedMessage> poll(std::chrono::milliseconds timeout);

  // confluent_kafka's `consumer.close()`.
  void close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace bazaartalks::streaming
