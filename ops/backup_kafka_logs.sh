#!/bin/bash
# Time-triggered (every 10 min) durability snapshot of the Kafka burn-in
# consumer logs -- distinct from rotate_kafka_logs.sh's SIZE-triggered
# rotation. This guarantees a gzip'd, timestamped copy of everything
# written is never more than ~10 minutes stale, independent of whether any
# log has crossed the rotation threshold -- protects against a consumer
# crash or disk issue corrupting/losing the live (unrotated) log between
# rotations.
#
# Snapshots land in logs/backups/<YYYYMMDD_HHMMSS>/, one gzip per source
# log. Older snapshots beyond RETENTION are pruned so this doesn't grow
# unbounded over a multi-day run.
set -euo pipefail

BACKUP_ROOT="/Users/umashankar/BazaarTalks-cpp/logs/backups"
RETENTION=144   # 144 snapshots * 10 min = 24h kept; older ones pruned

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

ts=$(date +%Y%m%d_%H%M%S)
dest="$BACKUP_ROOT/$ts"
mkdir -p "$dest"

copied=0
for f in "${LOGS[@]}"; do
  [ -f "$f" ] || continue
  base=$(basename "$f")
  cp "$f" "$dest/$base"
  gzip -f "$dest/$base"
  copied=$((copied + 1))
done

echo "[backup_kafka_logs] $(date '+%Y-%m-%d %H:%M:%S') snapshot -> $dest ($copied files)"

mkdir -p "$BACKUP_ROOT"
cd "$BACKUP_ROOT"
# shellcheck disable=SC2012
ls -1dt */ 2>/dev/null | tail -n +$((RETENTION + 1)) | while read -r old; do
  rm -rf "$old"
  echo "[backup_kafka_logs] pruned old snapshot $old"
done
