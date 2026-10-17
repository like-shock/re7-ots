#!/usr/bin/env bash
set -euo pipefail
pkg=$(cd -- "$(dirname -- "$0")" && pwd)
game=$(cd -- "${1:?Usage: bash install.sh /path/to/game-folder}" && pwd)
[[ -f "$game/re7.exe" ]] || { echo 're7.exe not found in game folder.' >&2; exit 1; }
if pgrep -x re7.exe >/dev/null 2>&1; then echo 'Close RE7 before installing.' >&2; exit 1; fi
(cd "$pkg" && sha256sum -c SHA256SUMS)
native=reframework/plugins/RE7_OTS_Native.dll
state="$game/reframework/data/RE7_OTS_Native/install"
[[ ! -e "$game/$native" && ! -L "$game/$native" ]] || { echo 'Native already exists; uninstall it first.' >&2; exit 1; }
old=(reframework/plugins/RE7_OTS_Explore.dll reframework/autorun/re7_ots_explore.lua reframework/plugins/RE7_OTS_Observer.dll reframework/plugins/RE7_OTS_Diagnostics.dll reframework/plugins/RE7_OTS_NativeProbe.dll)
for name in "${old[@]}"; do
 if [[ -e "$game/$name" || -L "$game/$name" ]]; then
  [[ -f "$game/$name" && ! -L "$game/$name" ]] || { echo "Refusing non-regular file: $name" >&2; exit 1; }
 fi
done
mkdir -p -- "$(dirname -- "$state")"
mkdir -- "$state" || { echo 'Existing install state; use uninstall.sh first.' >&2; exit 1; }
moved=(); copied=0
rollback() {
 trap - ERR INT TERM
 local failed=0
 if ((copied)); then rm -f -- "$game/$native" || failed=1; fi
 for name in "${old[@]}"; do
  [[ -f "$state/backup/$name" ]] || continue
  if [[ -e "$game/$name" || -L "$game/$name" ]]; then failed=1
  else mv -- "$state/backup/$name" "$game/$name" || failed=1; fi
 done
 if ((failed==0)); then rm -rf -- "$state"; else echo "Recovery files remain at: $state" >&2; fi
 exit 1
}
trap rollback ERR INT TERM
: > "$state/backups.list"; : > "$state/backups.sha256"
for name in "${old[@]}"; do
 if [[ -f "$game/$name" ]]; then
  mkdir -p -- "$state/backup/$(dirname -- "$name")"
  mv -- "$game/$name" "$state/backup/$name"; moved+=("$name")
  printf '%s\n' "$name" >> "$state/backups.list"
  (cd "$state/backup" && sha256sum -- "$name") >> "$state/backups.sha256"
 fi
done
mkdir -p -- "$game/reframework/plugins"
# Copy to private state first; move to destination only after complete copy/hash.
cp -- "$pkg/$native" "$state/native.pending"
sha256sum -- "$state/native.pending" | cut -d' ' -f1 > "$state/native.sha256"
cp -- "$pkg/uninstall.sh" "$state/uninstall.sh"
copied=1
mv -- "$state/native.pending" "$game/$native"
trap - ERR INT TERM
echo 'Experimental native OTS installed. Starts disabled; F6 toggles third person, F7 changes shoulder.'
echo 'Diagnostics are automatic: reframework/data/RE7_OTS_Native/native_*.jsonl'
echo "Restore after exiting the game: bash \"$state/uninstall.sh\" \"$game\""
