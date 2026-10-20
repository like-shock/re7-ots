#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_core.c src/core.c -o build/test_core
build/test_core
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_observer.c src/observer.c src/core.c -o build/test_observer
build/test_observer
bash original_build.sh
bash probe_build.sh
bash native_build.sh
bash explore_build.sh
python3 -m unittest discover -s tests -v
