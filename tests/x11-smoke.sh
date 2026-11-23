set -eu
check_window() {
    title=$1
    page=$2
    shift 2
    "$@" &
    pid=$!
    trap 'kill "$pid" 2>/dev/null || true' EXIT
    window=$(timeout 10s xdotool search --sync --onlyvisible --name "$title")
    xdotool windowfocus "$window"
    if [ -n "$page" ]; then
        xdotool key "$page"
    fi
    xdotool key Escape
    timeout 10s tail --pid="$pid" -f /dev/null
    wait "$pid"
    trap - EXIT
}

for game in build/river_flight.nxg build/crossing.nxg build/bounce.nxg; do
    check_window '^NEXIR 0.7$' '' ./nexir-gui "$game"
done
check_window '^NEXIR Wallet$' '' ./nexir-runtime build/legacy/wallet.nx
for game in build/legacy/river.nxg build/legacy/river_fast.nxg build/legacy/river_training.nxg; do
    check_window 'Native Workspace$' F3 ./nexir-gui build/legacy/wallet.nx "$game"
done
