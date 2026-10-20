#!/usr/bin/env bash
set -euo pipefail
game=$(cd -- "${1:?Usage: bash uninstall.sh /path/to/game-folder}" && pwd)
[[ -f "$game/re7.exe" ]] || { echo 're7.exe not found.' >&2; exit 1; }
if pgrep -x re7.exe >/dev/null 2>&1; then echo 'Close RE7 before restoring.' >&2; exit 1; fi
native=reframework/plugins/RE7_OTS_Native.dll
state="$game/reframework/data/RE7_OTS_Native/install"
[[ -f "$state/native.sha256" && -f "$state/backups.list" ]] || { echo 'Install receipt missing.' >&2; exit 1; }
if [[ -e "$game/$native" || -L "$game/$native" ]]; then
 [[ -f "$game/$native" && ! -L "$game/$native" ]] || { echo 'Native is not a regular file.' >&2; exit 1; }
 actual=$(sha256sum -- "$game/$native" | cut -d' ' -f1)
 [[ "$actual" == "$(cat "$state/native.sha256")" ]] || { echo 'Native was modified; leaving all files unchanged.' >&2; exit 1; }
fi
mapfile -t names < "$state/backups.list"
for name in "${names[@]}"; do
 case "$name" in
 reframework/plugins/RE7_OTS_Explore.dll|reframework/autorun/re7_ots_explore.lua|reframework/plugins/RE7_OTS_Observer.dll|reframework/plugins/RE7_OTS_Diagnostics.dll|reframework/plugins/RE7_OTS_NativeProbe.dll) ;;
 *) echo 'Invalid receipt path.' >&2; exit 1;;
 esac
 [[ ! -e "$game/$name" && ! -L "$game/$name" ]] || { echo "Restore destination occupied: $name" >&2; exit 1; }
 [[ -f "$state/backup/$name" && ! -L "$state/backup/$name" ]] || { echo "Backup missing: $name" >&2; exit 1; }
done
if ((${#names[@]})); then (cd "$state/backup" && sha256sum -c "$state/backups.sha256"); fi
restored=()
rollback() {
 trap - ERR INT TERM
 for name in "${names[@]}"; do
  if [[ ! -e "$state/backup/$name" && -f "$game/$name" ]]; then mv -- "$game/$name" "$state/backup/$name" || true; fi
 done
 echo "Restore interrupted; receipts retained at $state" >&2; exit 1
}
trap rollback ERR INT TERM
for name in "${names[@]}"; do mkdir -p -- "$game/$(dirname -- "$name")"; mv -- "$state/backup/$name" "$game/$name"; restored+=("$name"); done
rm -f -- "$game/$native"
trap - ERR INT TERM
rm -rf -- "$state"
echo 'Previous files restored. Captured logs were preserved.'
