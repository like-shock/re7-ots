#!/usr/bin/env bash
# The default suite includes both the preserved scanner and the state observer.
set -euo pipefail
exec bash "$(dirname "$0")/observer_test.sh"
