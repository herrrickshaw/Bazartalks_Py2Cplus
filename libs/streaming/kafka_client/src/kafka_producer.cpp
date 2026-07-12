#include "bazaartalks/streaming/kafka_producer.hpp"

#include <librdkafka/rdkafkacpp.h>

#include <string>

namespace bazaartalks::streaming {

namespace {

// Mirrors _emit_cdc()'s `_on_delivery` callback: records the error for the
// most recent in-flight message. publish_and_flush() is synchronous (it
// blocks in flush() until delivery completes or times out), so there is
// never more than one pending delivery report per KafkaProducer instance at
// a time -- one tracker per producer, reset before each publish, is enough.
class DeliveryReportTracker : public RdKafka::DeliveryReportCb {
 public:
  void dr_cb(RdKafka::Message& message) override {
    if (message.err() != RdKafka::ERR_NO_ERROR) {
      last_error_ = message.errstr();
    }
  }

  void reset() { last_error_.clear(); }
  bool has_error() const { return !last_error_.empty(); }
  const std::string& error() const { return last_error_; }

 private:
  std::string last_error_;
};

}  // namespace

struct KafkaProducer::Impl {
  std::unique_ptr<RdKafka::Conf> conf;
  std::unique_ptr<RdKafka::Producer> producer;
  DeliveryReportTracker tracker;
};

KafkaProducer::KafkaProducer(const std::string& bootstrap_servers) : impl_(new Impl) {
  impl_->conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  std::string errstr;
  if (impl_->conf->set("bootstrap.servers", bootstrap_servers, errstr) != RdKafka::Conf::CONF_OK) {
    throw CdcPublishError("failed to configure Kafka producer: " + errstr);
  }
  if (impl_->conf->set("dr_cb", &impl_->tracker, errstr) != RdKafka::Conf::CONF_OK) {
    throw CdcPublishError("failed to configure Kafka delivery callback: " + errstr);
  }

  impl_->producer.reset(RdKafka::Producer::create(impl_->conf.get(), errstr));
  if (!impl_->producer) {
    throw CdcPublishError("failed to construct Kafka producer: " + errstr);
  }
}

KafkaProducer::~KafkaProducer() {
  if (impl_ && impl_->producer) impl_->producer->flush(1000);
}

void KafkaProducer::publish_and_flush(const std::string& topic, const std::string& key,
                                       const std::string& value,
                                       std::chrono::milliseconds timeout) {
  impl_->tracker.reset();

  RdKafka::ErrorCode resp = impl_->producer->produce(
      topic, RdKafka::Topic::PARTITION_UA, RdKafka::Producer::RK_MSG_COPY,
      const_cast<char*>(value.data()), value.size(), key.data(), key.size(), 0, nullptr);
  if (resp != RdKafka::ERR_NO_ERROR) {
    throw CdcPublishError("failed to enqueue CDC event for key '" + key +
                           "': " + RdKafka::err2str(resp));
  }

  RdKafka::ErrorCode flush_resp = impl_->producer->flush(static_cast<int>(timeout.count()));
  if (flush_resp == RdKafka::ERR__TIMED_OUT) {
    int remaining = impl_->producer->outq_len();
    throw CdcPublishError("CDC event for key '" + key + "' not delivered within " +
                           std::to_string(timeout.count()) + "ms (" + std::to_string(remaining) +
                           " still queued)");
  }

  if (impl_->tracker.has_error()) {
    throw CdcPublishError("CDC event for key '" + key +
                           "' rejected by broker: " + impl_->tracker.error());
  }
}

}  // namespace bazaartalks::streaming
