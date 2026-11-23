set -eu
for game in build/crossing.nxg build/bounce.nxg; do
    ./nexir-gui "$game" &
    pid=$!
    trap 'kill "$pid" 2>/dev/null || true' EXIT
    window=$(timeout 10s xdotool search --sync --onlyvisible --pid "$pid" --name '^NEXIR 0.7$')
    xdotool windowfocus "$window" key Escape
    timeout 10s tail --pid="$pid" -f /dev/null
    wait "$pid"
    trap - EXIT
done
