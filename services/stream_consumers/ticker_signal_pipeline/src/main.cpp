// Thin CLI/poll-loop orchestration for stream_pipeline.py's C++ port --
// data-plumbing, not tested here, same boundary as cdc_consumer/
// signal_consumer's main.cpp. `_universe()`'s Excel-file discovery is out
// of scope (per-ticker discovery is passed via --tickers instead); OHLC
// retrieval uses the already-ported MarketStore::get_ohlc() (Phase 2),
// matching the Python original's market_store.get_ohlc()/cached_download()
// call -- minus the yfinance cache-miss fallback, which stays in the
// Python sidecar per that module's own scope boundary.
#include "bazaartalks/services/ticker_signal_pipeline.hpp"
#include "bazaartalks/storage/market_store.hpp"
#include "bazaartalks/streaming/kafka_consumer.hpp"
#include "bazaartalks/streaming/kafka_producer.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace bazaartalks::services::ticker_signal_pipeline;
using bazaartalks::storage::MarketStore;
using bazaartalks::streaming::KafkaConsumer;
using bazaartalks::streaming::KafkaProducer;

namespace {
constexpr const char* kTopicWork = "scan.tickers";
constexpr const char* kTopicResult = "scan.signals";
constexpr const char* kBroker = "localhost:9092";

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) out.push_back(item);
  }
  return out;
}

void produce(const std::vector<std::string>& tickers) {
  KafkaProducer producer(kBroker);
  for (const auto& t : tickers) {
    producer.publish_and_flush(kTopicWork, t, t);
  }
  std::cerr << "produced " << tickers.size() << " tickers -> " << kTopicWork << std::endl;
}

void consume(const std::string& group) {
  KafkaConsumer consumer(kBroker, group);
  consumer.subscribe({kTopicWork});
  KafkaProducer producer(kBroker);
  MarketStore store;

  int n = 0, idle = 0;
  std::cerr << "[worker " << group << "] consuming " << kTopicWork << "..." << std::endl;
  while (idle < 8) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (!msg) {
      ++idle;
      continue;
    }
    if (msg->has_error) continue;
    idle = 0;

    std::string ticker = msg->value;
    std::vector<double> close;
    for (const auto& bar : store.get_ohlc(ticker)) close.push_back(bar.c);
    auto result = compute_ticker_signal(ticker, close);
    producer.publish_and_flush(kTopicResult, result.ticker, to_json(result));
    ++n;
    if (n % 25 == 0) std::cerr << "[worker " << group << "] processed " << n << std::endl;
  }
  std::cerr << "[worker " << group << "] done, " << n << " signals -> " << kTopicResult
            << std::endl;
}

void demo() {
  std::vector<std::string> tickers = {"AAPL", "MSFT", "NVDA", "GOOGL", "AMZN"};
  produce(tickers);

  KafkaConsumer consumer(kBroker, "demo");
  consumer.subscribe({kTopicWork});
  MarketStore store;
  int got = 0, idle = 0;
  while (got < static_cast<int>(tickers.size()) && idle < 10) {
    auto msg = consumer.poll(std::chrono::milliseconds(1000));
    if (!msg) {
      ++idle;
      continue;
    }
    if (msg->has_error) continue;
    std::vector<double> close;
    for (const auto& bar : store.get_ohlc(msg->value)) close.push_back(bar.c);
    auto result = compute_ticker_signal(msg->value, close);
    std::cout << "  signal: " << to_json(result) << std::endl;
    ++got;
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: bt_stream_pipeline <produce --tickers A,B,C | consume --group G | demo>"
              << std::endl;
    return 1;
  }
  std::string cmd = argv[1];
  if (cmd == "produce") {
    std::vector<std::string> tickers;
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]) == "--tickers" && i + 1 < argc) tickers = split_csv(argv[++i]);
    }
    produce(tickers);
  } else if (cmd == "consume") {
    std::string group = "scan1";
    for (int i = 2; i < argc; ++i) {
      if (std::string(argv[i]) == "--group" && i + 1 < argc) group = argv[++i];
    }
    consume(group);
  } else {
    demo();
  }
  return 0;
}
