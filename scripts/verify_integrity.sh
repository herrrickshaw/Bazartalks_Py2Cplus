#!/usr/bin/env bash
# Re-hashes every git-tracked file and diffs against the committed
# CHECKSUMS.sha256 manifest. Same mechanism as the original BazaarTalks
# repo's verify_integrity.sh, ported for this repo's tracked-file set.
set -euo pipefail
cd "$(git rev-parse --show-toplevel)"

git ls-files | sort | grep -vxF "CHECKSUMS.sha256" | xargs shasum -a 256 > /tmp/CHECKSUMS.current.sha256

if ! diff -u CHECKSUMS.sha256 /tmp/CHECKSUMS.current.sha256; then
  echo "Integrity check FAILED: tracked files differ from CHECKSUMS.sha256." >&2
  echo "If this change is intentional, regenerate the manifest:" >&2
  echo '  git ls-files | sort | grep -vxF "CHECKSUMS.sha256" | xargs shasum -a 256 > CHECKSUMS.sha256' >&2
  exit 1
fi
echo "Integrity check passed."
