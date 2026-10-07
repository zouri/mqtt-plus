#!/bin/bash
set -euo pipefail

if [[ $(uname -s) != Darwin || $# -lt 1 || $# -gt 4 ]]; then
    echo "Usage: bash scripts/benchmark-cpu-macos.sh EXECUTABLE [live|live-varied|paused|paused-varied|idle|connected-idle|broker-populated-idle|broker-live-varied|broker-paused-varied|broker-scroll-varied] [max-cpu-percent] [seconds=30]" >&2
    exit 2
fi
app=$1
mode=${2:-live}
budget=${3:-20}
duration=${4:-30}
if [[ ! -x $app || ! $mode =~ ^(live|live-varied|paused|paused-varied|idle|connected-idle|broker-populated-idle|broker-live-varied|broker-paused-varied|broker-scroll-varied)$ ]]; then
    echo "Provide a profiling-enabled executable and a valid scenario" >&2
    exit 2
fi
broker_input=false
activate_window=${MQTT_PROFILE_ACTIVATE_WINDOW:-0}
background_window=${MQTT_PROFILE_BACKGROUND_WINDOW:-0}
capture_window=${MQTT_PROFILE_CAPTURE_WINDOW:-0}
if [[ ($activate_window != 0 && $activate_window != 1)
    || ($background_window != 0 && $background_window != 1)
    || ($capture_window != 0 && $capture_window != 1)
    || ($activate_window = 1 && $background_window = 1) ]]; then
    echo "Window controls must be 0 or 1, and foreground/background are mutually exclusive" >&2
    exit 2
fi
if [[ $mode = broker-* ]]; then broker_input=true; fi
if [[ ! $duration =~ ^[1-9][0-9]?$ || $duration -gt 40 || ! $budget =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    echo "Duration must be 1-40 seconds and CPU budget must be non-negative" >&2
    exit 2
fi
if [[ $mode = connected-idle && $duration -lt 30 ]]; then
    echo "connected-idle requires 30-40 seconds to observe the 30-second keepalive" >&2
    exit 2
fi
for dependency in rg jq; do
    if ! command -v "$dependency" >/dev/null; then
        echo "Required command not found: $dependency" >&2
        exit 2
    fi
done
if ! LC_ALL=C rg -a -F 'PROFILE_MESSAGE_STREAM settings isolated format=ini updates=disabled' "$app" >/dev/null; then
    echo "Executable lacks temporary INI settings isolation; rebuild before benchmarking" >&2
    exit 2
fi
if $broker_input && ! LC_ALL=C rg -a -F 'brokerInput=' "$app" >/dev/null; then
    echo "Executable lacks real broker input profiling; rebuild before benchmarking" >&2
    exit 2
fi
if [[ $mode = broker-scroll-varied ]] && ! LC_ALL=C rg -a -F 'scrollOutput=' "$app" >/dev/null; then
    echo "Executable lacks wheel-input profiling; rebuild before benchmarking" >&2
    exit 2
fi
if [[ $activate_window = 1 ]] && ! LC_ALL=C rg -a -F 'activateWindow=' "$app" >/dev/null; then
    echo "Executable lacks active-window observation; rebuild before benchmarking" >&2
    exit 2
fi
if [[ $activate_window = 1 ]] && ! LC_ALL=C rg -a -F 'foregroundReady=' "$app" >/dev/null; then
    echo "Executable lacks foreground warmup verification; rebuild before benchmarking" >&2
    exit 2
fi
if [[ $background_window = 1 ]] && ! LC_ALL=C rg -a -F 'backgroundWindow=' "$app" >/dev/null; then
    echo "Executable lacks background-window control; rebuild before benchmarking" >&2
    exit 2
fi
if [[ $capture_window = 1 ]] && ! LC_ALL=C rg -a -F 'windowImageCaptured' "$app" >/dev/null; then
    echo "Executable lacks isolated window capture support; rebuild before capturing" >&2
    exit 2
fi

count=$(( (duration + 10) * 100 ))
args=(--profile-message-stream --profile-message-count "$count" --profile-message-batch 1 --profile-message-interval 10 --profile-payload-bytes 2048)
if [[ $mode = paused* || $mode = broker-paused-varied ]]; then args+=(--profile-pause-output); fi
if [[ $mode = *-varied || $mode = broker-populated-idle ]]; then args+=(--profile-vary-payload); fi
if $broker_input; then args+=(--profile-broker-input); fi
if [[ $mode = broker-scroll-varied ]]; then args+=(--profile-scroll-output); fi
if [[ $mode = broker-populated-idle ]]; then
    count=4000
    budget=${3:-3}
    args=(--profile-message-stream --profile-message-count "$count" --profile-payload-bytes 2048
        --profile-vary-payload --profile-broker-input
        --profile-observation-ms "$(( (duration + 50) * 1000 ))")
elif [[ $mode = *idle ]]; then
    count=1
    budget=${3:-3}
    args=(--profile-message-stream --profile-message-count 1 --profile-observation-ms "$(( (duration + 10) * 1000 ))")
fi
if [[ $activate_window = 1 ]]; then args+=(--profile-activate-window); fi
if [[ $background_window = 1 ]]; then args+=(--profile-background-window); fi
if [[ $capture_window = 1 ]]; then args+=(--profile-capture-window); fi
# Process-local Cocoa override; do not change persistent user defaults.
args+=(-NSAppSleepDisabled YES)
log=$(mktemp "${TMPDIR:-/tmp}/mqtt-cpu-benchmark.XXXXXX")
pid=
broker_pid=
publisher_pid=
generator_pid=
script_dir=$(cd "$(dirname "$0")" && pwd)
cleanup() {
    local child parent
    for child in "$generator_pid" "$publisher_pid" "$pid" "$broker_pid"; do
        if [[ -z $child ]]; then continue; fi
        parent=$(ps -p "$child" -o ppid= 2>/dev/null || true)
        parent=${parent//[[:space:]]/}
        if [[ $parent = "$$" ]]; then kill "$child" 2>/dev/null || true; fi
        wait "$child" 2>/dev/null || true
    done
}
trap cleanup EXIT
wait_for_publisher() {
    if ! wait "$generator_pid" || ! wait "$publisher_pid"; then
        echo "Synthetic publisher failed; inspect $publisher_dir" >&2
        exit 2
    fi
    generator_pid=
    publisher_pid=
}
check_foreground_ready() {
    for ((attempt=0; attempt<50; ++attempt)); do
        if rg -q 'PROFILE_MESSAGE_STREAM foregroundReady=' "$log"; then break; fi
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    if ! rg -q 'PROFILE_MESSAGE_STREAM foregroundReady= true' "$log"; then
        echo "Window not foreground after warmup; CPU measurement was not started; inspect $log" >&2
        exit 2
    fi
}
if [[ $mode = connected-idle ]] || $broker_input; then
    if ! command -v mosquitto >/dev/null; then
        echo "connected-idle requires a local mosquitto executable" >&2
        exit 2
    fi
    port=$(perl -MIO::Socket::INET -e '
        my $socket = IO::Socket::INET->new(LocalAddr => "127.0.0.1", LocalPort => 0,
            Proto => "tcp", Listen => 1) or die "Cannot reserve local port: $!";
        print $socket->sockport;
    ')
    config=$(mktemp "${TMPDIR:-/tmp}/mqtt-local-broker.XXXXXX")
    broker_log=$(mktemp "${TMPDIR:-/tmp}/mqtt-local-broker-log.XXXXXX")
    printf 'listener %s 127.0.0.1\nallow_anonymous true\npersistence false\nlog_type all\n' "$port" >"$config"
    mosquitto -c "$config" >"$broker_log" 2>&1 &
    broker_pid=$!
    sleep 1
    if ! kill -0 "$broker_pid" 2>/dev/null || ! rg -q 'mosquitto version .* running' "$broker_log"; then
        echo "Isolated local broker failed to start; inspect $broker_log" >&2
        exit 2
    fi
    args+=(--profile-local-broker-port "$port")
    echo "Local broker log: $broker_log"
fi
"$app" "${args[@]}" >"$log" 2>&1 &
pid=$!
if $broker_input; then
    if ! command -v mosquitto_pub >/dev/null; then
        echo "Broker traffic requires mosquitto_pub" >&2
        exit 2
    fi
    for ((attempt=0; attempt<150; ++attempt)); do
        if rg -q 'PROFILE_MESSAGE_STREAM started.*brokerInput= true' "$log"; then break; fi
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    if ! rg -q 'PROFILE_MESSAGE_STREAM started.*brokerInput= true' "$log"; then
        echo "Broker subscription did not become ready; inspect $log" >&2
        exit 2
    fi
    publisher_dir=$(mktemp -d "${TMPDIR:-/tmp}/mqtt-profile-publisher.XXXXXX")
    mkfifo "$publisher_dir/messages"
    HOME="$publisher_dir" XDG_CONFIG_HOME="$publisher_dir" mosquitto_pub -o /dev/null \
        -h 127.0.0.1 -p "$port" -t profile/topic-0 -V mqttv5 -q 0 --nodelay -l \
        <"$publisher_dir/messages" >"$publisher_dir/publisher.log" 2>&1 &
    publisher_pid=$!
    perl "$script_dir/generate-profile-json.pl" "$count" 2048 \
        >"$publisher_dir/messages" 2>"$publisher_dir/generator.log" &
    generator_pid=$!
    echo "Publisher logs: $publisher_dir"
fi
if [[ $mode = broker-populated-idle ]]; then
    if [[ $activate_window = 1 ]]; then check_foreground_ready; fi
    wait_for_publisher
    for ((attempt=0; attempt<50; ++attempt)); do
        if rg -q 'PROFILE_MESSAGE_STREAM injection complete' "$log"; then break; fi
        if ! kill -0 "$pid" 2>/dev/null; then break; fi
        sleep 0.1
    done
    if ! rg -q 'PROFILE_MESSAGE_STREAM injection complete' "$log"; then
        echo "Population did not finish before idle warmup; inspect $log" >&2
        exit 2
    fi
    echo "Populated idle: publisher exited; 4000 messages admitted before warmup"
fi
sleep 5
if [[ $activate_window = 1 ]]; then check_foreground_ready; fi
if ! rg -q 'PROFILE_MESSAGE_STREAM started' "$log" ||
    ! rg -q 'PROFILE_MESSAGE_STREAM settings isolated format=ini updates=disabled' "$log"; then
    echo "Synthetic driver did not start; inspect $log" >&2
    exit 2
fi
result=0
bash "$script_dir/measure-cpu-macos.sh" "$pid" "$duration" "$budget" || result=$?
if $broker_input && [[ -n $generator_pid ]]; then
    wait_for_publisher
fi
for ((attempt=0; attempt<60; ++attempt)); do
    if rg -q 'PROFILE_MESSAGE_STREAM summary' "$log"; then break; fi
    if ! kill -0 "$pid" 2>/dev/null; then break; fi
    sleep 1
done
summary=$(sed -n 's/^PROFILE_MESSAGE_STREAM summary //p' "$log")
if [[ -z $summary ]]; then
    echo "No recovery summary; inspect $log" >&2
    exit 2
fi
if $broker_input && ! jq -e '
    .brokerInput == true and .localBrokerPort > 0
    and .brokerConnected == true and .brokerStayedConnected == true
    ' <<<"$summary" >/dev/null; then
    echo "Real broker input or connection check failed; inspect $log" >&2
    exit 1
fi
echo "$summary"
if [[ $capture_window = 1 ]] && ! jq -e '
    .windowImageCaptured == true and .windowImageWidth > 0 and .windowImageHeight > 0
    ' <<<"$summary" >/dev/null; then
    echo "Isolated application window capture failed; inspect $log" >&2
    exit 1
fi
if [[ $activate_window = 1 ]] && ! jq -e '
    .activateWindow == true and .activeWindowSamplesAfterWarmup > 0
    and .inactiveWindowSamplesAfterWarmup == 0 and .windowFinallyActive == true
    and .windowLostActivationAfterWarmup == false
    ' <<<"$summary" >/dev/null; then
    echo "The window did not stay active after warmup; foreground CPU comparison is invalid" >&2
    exit 1
fi
if [[ $background_window = 1 ]] && ! jq -e '
    .backgroundWindow == true and .windowDoesNotAcceptFocus == true
    and .inactiveWindowSamplesAfterWarmup > 0 and .activeWindowSamplesAfterWarmup == 0
    and .windowFinallyActive == false
    ' <<<"$summary" >/dev/null; then
    echo "The window did not stay inactive after warmup; background comparison is invalid" >&2
    exit 1
fi
if ! jq -e --argjson count "$count" '
    .sentMessages == $count and .totalMessageCount == $count
    and .persistedMessageCount == $count and .persistedSuccessfulParses == $count
    and .droppedMessages == 0 and .droppedParseResults == 0
    and .droppedParseTasks == 0 and .pressureSkippedParses == 0
    and .finalState == "normal"' <<<"$summary" >/dev/null; then
    echo "Message integrity or recovery check failed; inspect $log" >&2
    exit 1
fi
if [[ $mode != *idle || $mode = broker-populated-idle ]] && ! jq -e '
    .injectionMessagesPerSecond >= 95 and .injectionMessagesPerSecond <= 105
    ' <<<"$summary" >/dev/null; then
    echo "Input rate drifted outside 95-105 messages/s; CPU comparison is invalid" >&2
    exit 1
fi
if [[ $mode = connected-idle ]] && ! jq -e --argjson minimum "$(( (duration + 10) * 1000 ))" '
    .localBrokerPort > 0 and .brokerConnected == true
    and .brokerStayedConnected == true and .renderObservationMs >= $minimum
    ' <<<"$summary" >/dev/null; then
    echo "The isolated connection did not remain connected for the observation; inspect $log" >&2
    exit 1
fi
if [[ $mode = connected-idle || $mode = broker-populated-idle ]] && { ! rg -q 'Received PINGREQ' "$broker_log" || ! rg -q 'Sending PINGRESP' "$broker_log"; }; then
    echo "The isolated broker did not confirm a keepalive round trip; inspect $broker_log" >&2
    exit 1
fi
if [[ $mode = *-varied || $mode = broker-populated-idle ]] && ! jq -e --argjson count "$count" '
    .varyPayload == true and .verifiedPayloadSequences == $count
    ' <<<"$summary" >/dev/null; then
    echo "Variable-payload sequence verification failed; inspect $log" >&2
    exit 1
fi
if [[ $mode = broker-populated-idle ]] && ! jq -e --argjson minimum "$(( (duration + 50) * 1000 ))" '
    .brokerInput == true and .renderObservationMs >= $minimum
    and .visibleMessageCount == 1200
    ' <<<"$summary" >/dev/null; then
    echo "Populated idle did not retain the saturated message window; inspect $log" >&2
    exit 1
fi
if [[ $mode = live* || $mode = *idle || $mode = broker-live-varied ]] && ! jq -e '
    .followTailChecked == true and .followTailVisible == true
    ' <<<"$summary" >/dev/null; then
    echo "The latest message is not visible at the followed tail; inspect $log" >&2
    exit 1
fi
if [[ $mode = broker-scroll-varied ]] && ! jq -e --argjson seconds "$duration" '
    .scrollOutput == true and .scrollEvents >= 100
    and .manualScrollEvents >= .scrollEvents * 0.9
    and .scrollDistance >= 1000 and .scrollWasManual == true
    and .scrollListVisible == true and .scrollMessageVisible == true
    and ($seconds < 30 or .scrollReversals >= 1)
    ' <<<"$summary" >/dev/null; then
    echo "Wheel events did not produce visible manual scrolling; inspect $log" >&2
    exit 1
fi
echo "Message integrity and recovery: PASS; raw log: $log"
exit "$result"
