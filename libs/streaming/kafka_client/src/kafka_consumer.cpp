#include "bazaartalks/streaming/kafka_consumer.hpp"

#include <librdkafka/rdkafkacpp.h>

namespace bazaartalks::streaming {

struct KafkaConsumer::Impl {
  std::unique_ptr<RdKafka::Conf> conf;
  std::unique_ptr<RdKafka::KafkaConsumer> consumer;
};

KafkaConsumer::KafkaConsumer(const std::string& bootstrap_servers, const std::string& group_id,
                             const std::string& auto_offset_reset)
    : impl_(new Impl) {
  impl_->conf.reset(RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL));
  std::string errstr;
  if (impl_->conf->set("bootstrap.servers", bootstrap_servers, errstr) != RdKafka::Conf::CONF_OK) {
    throw KafkaConsumerError("failed to configure Kafka consumer: " + errstr);
  }
  if (impl_->conf->set("group.id", group_id, errstr) != RdKafka::Conf::CONF_OK) {
    throw KafkaConsumerError("failed to configure Kafka consumer group.id: " + errstr);
  }
  if (impl_->conf->set("auto.offset.reset", auto_offset_reset, errstr) != RdKafka::Conf::CONF_OK) {
    throw KafkaConsumerError("failed to configure Kafka consumer auto.offset.reset: " + errstr);
  }

  impl_->consumer.reset(RdKafka::KafkaConsumer::create(impl_->conf.get(), errstr));
  if (!impl_->consumer) {
    throw KafkaConsumerError("failed to construct Kafka consumer: " + errstr);
  }
}

KafkaConsumer::~KafkaConsumer() {
  if (impl_ && impl_->consumer) impl_->consumer->close();
}

void KafkaConsumer::subscribe(const std::vector<std::string>& topics) {
  RdKafka::ErrorCode err = impl_->consumer->subscribe(topics);
  if (err != RdKafka::ERR_NO_ERROR) {
    throw KafkaConsumerError("failed to subscribe: " + RdKafka::err2str(err));
  }
}

std::optional<ConsumedMessage> KafkaConsumer::poll(std::chrono::milliseconds timeout) {
  std::unique_ptr<RdKafka::Message> msg(
      impl_->consumer->consume(static_cast<int>(timeout.count())));

  if (msg->err() == RdKafka::ERR__TIMED_OUT) {
    return std::nullopt;  // confluent_kafka: msg is None
  }

  ConsumedMessage out;
  if (msg->err() != RdKafka::ERR_NO_ERROR) {
    out.has_error = true;
    out.error_str = msg->errstr();
    return out;
  }

  if (msg->key()) out.key = *msg->key();
  if (msg->payload()) {
    out.value.assign(static_cast<const char*>(msg->payload()), msg->len());
  }
  return out;
}

void KafkaConsumer::close() {
  if (impl_->consumer) {
    impl_->consumer->close();
    impl_->consumer.reset();
  }
}

}  // namespace bazaartalks::streaming
