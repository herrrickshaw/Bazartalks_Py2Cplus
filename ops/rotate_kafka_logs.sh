#!/bin/bash
# Copytruncate-style rotation for the BazaarTalks Kafka consumer burn-in logs.
#
# newsyslog(8) refuses to run without root even for user-owned files, so this
# is a small user-level replacement. It truncates each log IN PLACE (via
# ": > file", not mv/rm) after gzipping a copy -- the running consumer
# processes hold their StandardOutPath/StandardErrorPath file descriptors
# open for the process lifetime and don't reopen on SIGHUP, so renaming the
# file out from under them would just have them keep appending to the
# renamed (now-hidden) file forever. Truncating the same inode is the only
# rotation shape that works without restarting the 4 consumer services.
#
# Usage: rotate_kafka_logs.sh [--dry-run]
set -euo pipefail

THRESHOLD_KB=10240
MAX_GENERATIONS=5
DRY_RUN=0
[ "${1:-}" = "--dry-run" ] && DRY_RUN=1

LOGS=(
  "/Users/umashankar/BazaarTalks/kafka_cdc_consumer.log"
  "/Users/umashankar/BazaarTalks/kafka_cdc_consumer.err.log"
  "/Users/umashankar/BazaarTalks/kafka_signal_consumer.log"
  "/Users/umashankar/BazaarTalks/kafka_signal_consumer.err.log"
  "/Users/umashankar/BazaarTalks-cpp/logs/bt_cdc_consumer.log"
  "/Users/umashankar/BazaarTalks-cpp/logs/bt_cdc_consumer.err.log"
  "/Users/umashankar/BazaarTalks-cpp/logs/bt_signal_consumer.log"
  "/Users/umashankar/BazaarTalks-cpp/logs/bt_signal_consumer.err.log"
)

for f in "${LOGS[@]}"; do
  [ -f "$f" ] || continue
  size_kb=$(( $(stat -f%z "$f") / 1024 ))
  if [ "$size_kb" -lt "$THRESHOLD_KB" ]; then
    continue
  fi

  echo "[rotate_kafka_logs] $f is ${size_kb}KB >= ${THRESHOLD_KB}KB, rotating"
  if [ "$DRY_RUN" = "1" ]; then
    continue
  fi

  for ((i = MAX_GENERATIONS - 1; i >= 1; i--)); do
    [ -f "$f.$i.gz" ] && mv -f "$f.$i.gz" "$f.$((i + 1)).gz"
  done
  rm -f "$f.$((MAX_GENERATIONS + 1)).gz"

  cp "$f" "$f.1"
  gzip -f "$f.1"
  : > "$f"
done
