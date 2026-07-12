// Thin CLI/poll-loop orchestration for kafka_cdc_consumer.py's C++ port --
// data-plumbing (argv parsing, the poll loop, signal handling, stdout
// logging), not tested here, matching every other phase's "main() is out
// of scope" boundary. The actual ported logic (parse_cdc, CdcWindowAggregator)
// lives in cdc_consumer.hpp/.cpp and is golden-tested in tests/.
#include "bazaartalks/services/cdc_consumer.hpp"
#include "bazaartalks/streaming/kafka_consumer.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>

using namespace bazaartalks::services::cdc_consumer;
using bazaartalks::streaming::KafkaConsumer;

namespace {
double now_seconds() { return static_cast<double>(std::time(nullptr)); }
}  // namespace

int main(int argc, char** argv) {
  double window_seconds = 30.0;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--window" && i + 1 < argc) {
      window_seconds = std::stod(argv[++i]);
    }
  }

  const char* bootstrap_env = std::getenv("KAFKA_BOOTSTRAP");
  std::string bootstrap = bootstrap_env ? bootstrap_env : "localhost:9092";
  const std::string topic = "ohlc.cdc";

  KafkaConsumer consumer(bootstrap, "kafka-cdc-consumer");
  consumer.subscribe({topic});

  CdcWindowAggregator agg(window_seconds, now_seconds());
  std::cout << "[kafka-cdc] consuming topic=" << topic << " bootstrap=" << bootstrap
            << " window=" << window_seconds << "s" << std::endl;

  while (true) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    double now = now_seconds();

    if (msg && !msg->has_error) {
      auto parsed = parse_cdc(msg->value);
      if (parsed) agg.add(parsed->ticker, parsed->n_bars);
    }

    if (agg.should_flush(now)) {
      for (const auto& [ticker, n] : agg.flush()) {
        std::cout << "[kafka-cdc] ticker=" << ticker << " bars_mutated_in_window=" << n
                  << std::endl;
      }
      agg.reset(now);
    }
  }
}
