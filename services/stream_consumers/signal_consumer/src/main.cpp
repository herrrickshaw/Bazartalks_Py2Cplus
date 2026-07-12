// Thin CLI/poll-loop orchestration -- data-plumbing, not tested here, same
// boundary as cdc_consumer's main.cpp.
#include "bazaartalks/services/signal_consumer.hpp"
#include "bazaartalks/streaming/kafka_consumer.hpp"

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

using namespace bazaartalks::services::signal_consumer;
using bazaartalks::streaming::KafkaConsumer;

namespace {
double now_seconds() { return static_cast<double>(std::time(nullptr)); }

std::string format_window_end(double window_start) {
  std::time_t t = static_cast<std::time_t>(window_start);
  std::tm local_tm{};
  localtime_r(&t, &local_tm);
  std::ostringstream oss;
  oss << std::put_time(&local_tm, "%Y-%m-%dT%H:%M:%S");
  return oss.str();
}
}  // namespace

int main(int argc, char** argv) {
  double window_seconds = 5.0;
  // Same default group id as kafka_signal_consumer.py's hardcoded
  // "kafka-signal-consumer" -- override via --group-id when running
  // alongside the Python original on the SAME topic, since two consumers
  // sharing one group would split partitions instead of each seeing
  // every message.
  std::string group_id = "kafka-signal-consumer";
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--window" && i + 1 < argc) {
      window_seconds = std::stod(argv[++i]);
    } else if (arg == "--group-id" && i + 1 < argc) {
      group_id = argv[++i];
    }
  }

  const char* bootstrap_env = std::getenv("KAFKA_BOOTSTRAP");
  std::string bootstrap = bootstrap_env ? bootstrap_env : "localhost:9092";
  const std::string topic = "scan.signals";

  KafkaConsumer consumer(bootstrap, group_id);
  consumer.subscribe({topic});

  SignalWindowAggregator agg(window_seconds, now_seconds());
  std::cout << "[kafka-signals] consuming topic=" << topic << " bootstrap=" << bootstrap
            << " window=" << window_seconds << "s" << std::endl;

  while (true) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    double now = now_seconds();

    if (msg && !msg->has_error) {
      auto parsed = parse_signal(msg->value);
      if (parsed) agg.add(*parsed);
    }

    if (agg.should_flush(now)) {
      std::string window_end = format_window_end(now);
      for (const auto& [signal, n] : agg.flush()) {
        std::cout << "[kafka-signals] window_end=" << window_end << " signal=" << signal
                  << " n=" << n << std::endl;
      }
      agg.reset(now);
    }
  }
}
