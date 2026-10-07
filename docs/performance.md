# CPU Measurement And Acceptance

Current retained changes: 33 ms visible-row batches, deduplicated follow-tail
requests, lazy row actions and explicit width hints, exact model change roles,
adaptive display-count timing and synchronization with visible batches, and a
macOS ASCII-only compact-payload rendering path. Controlled paired measurements
showed a 32.3% relative paused-count improvement, 9.3% for ASCII live rendering
and 9.4% for live counter synchronization. These use different baselines and
must not be added. Whole-product idle/live/scroll competitor acceptance remains
unproven. Later sections supersede the early exploratory results below.

**Evidence correction:** measurements recorded before explicit temporary INI
settings isolation below did not isolate macOS native preferences. Qt test
mode redirected application data, but the organization's QSettings constructor
still used NativeFormat. Historical results are retained for transparency,
not treated as strictly isolated acceptance evidence. Synthetic profiling may
have modified ordinary session / subscription preferences; no unverified
automatic restoration or deletion of native preferences is attempted.

Status: **not accepted for release promotion**. Controlled synthetic incoming
traffic at 100 messages/s with 2 KB payloads measured a median 34.20% CPU before
footer refresh tuning. A footer-tuned live run measured 32.90%, and a later
frame-instrumented constant-payload run measured 32.53%;
paused output measured a median 13.12% in repeated footer A/B tests (50 ms
reference: 20.15%). Historical disconnected idle measured
0.80%. These are local measurements, not competitor comparison results.
Presentation optimizations and regression tests are in place, but the reported
same-broker idle / incoming / scrolling gap has not been eliminated or verified.

## Reproducible measurement

Use process CPU time, not an instantaneous Activity Monitor or `ps %cpu` reading:

```bash
bash scripts/measure-cpu-macos.sh PID 30
bash scripts/measure-cpu-macos.sh PID 30 20
```

The optional last argument is a local CPU budget, not a competitor comparison.
Exit codes: 0 = measured / within budget, 1 = over budget, 2 = invalid or
unavailable measurement. The script does not start, change, or stop the process.
It checks process identity, uses a monotonic elapsed-time clock, and calculates
`100 * process_cpu_seconds / elapsed_seconds`. 100% means one CPU core; values
can exceed 100% on a multithreaded process. Child processes are not included.

Measure each application sequentially on the same machine, with the same window
size, foreground visibility, broker, subscriptions, QoS, payloads, message rate,
history size, and display mode. Keep unrelated heavy workloads out of the
measurement. Warm up first, measure at least 30 seconds, repeat at least three
times, and retain every result rather than choosing the best run.

Test these scenarios separately:

1. Disconnected idle and connected idle with no traffic, no active animations.
2. Steady incoming traffic with the list following the newest message.
3. Steady incoming traffic with output paused (message-list control; sidebar,
   topic-rate and status updates still run, so this is not a backend-only test).
4. Manual scrolling through a populated history, including expanded payloads.

Verify stored counts, dropped messages, parsing outcomes, selection, keyboard
navigation, copy actions, and follow/resume behavior alongside CPU. Do not meet
a CPU budget by silently dropping messages or disabling required functionality.
Distinguish percentage points from relative changes when reporting comparisons.

## Release-like synthetic workloads

Normal Release builds do not expose the synthetic driver. Opt into a dedicated
profiling build without `QT_QML_DEBUG`, preserving normal QML caching behavior:

```bash
cmake -S . -B build/cpu-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_ROOT" \
  -DMQTT_PLUS_ENABLE_PROFILING=ON
cmake --build build/cpu-release
build/cpu-release/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app \
  --profile-message-stream --profile-message-count 10000 \
  --profile-message-batch 1 --profile-message-interval 10 \
  --profile-payload-bytes 2048
```

On macOS, set `QT_ROOT` to the local Qt installation. Other platforms use their
platform-specific executable path. The explicit workload flag redirects data
to Qt's test-mode location before constructing application services. Settings
use explicit IniFormat with both user and system scopes pointed at a fresh
temporary directory; automatic update checks are disabled there. Normal runs
retain NativeFormat. The runner requires the settings-isolation marker and
rejects older profiling builds. The synthetic `profile/#` subscription and
history are not the user's normal data in this corrected harness.
Runs share that test-mode location: run only one synthetic workload at a time.
Add `--profile-pause-output` for the processing control; use a count of 1 and
wait for the recovery summary for a quiescent populated-list scenario.
Close the test application after measuring. Do not use private session exports
as benchmark fixtures.

For a bounded, unattended check (macOS; requires `rg` and `jq`):

```bash
bash scripts/benchmark-cpu-macos.sh \
  build/cpu-release/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app live 20
```

The runner warms up for five seconds, measures thirty seconds without a
profiler attached, waits for recovery, and checks all 4,000 messages were kept
without drops or pressure-skipped parsing. It stops only its own test process.
Use `paused` for the processing control or `idle` for a single-message run.
Use `live-varied` or `paused-varied` for unique per-message JSON sequences with
the same payload size and an additional persisted-sequence integrity check.
The optional fourth argument changes the measurement duration (1-40 seconds);
the workload count is adjusted to keep traffic running throughout the window.
This keeps the bounded workload within the default 5,000-message retention
limit. A lower isolated retention setting will fail the persistence check.
Missing recovery evidence is a failed measurement, not a pass.
The runner passes `-NSAppSleepDisabled YES` as a process-local Cocoa argument
for this synthetic test only; it does not write persistent user defaults.
It also rejects incoming-traffic runs outside 95-105 messages/s. Otherwise
background timer throttling can make a low CPU result incomparable with a
foreground application. Keep window visibility consistent even with this
override. This is not a change to normal application energy behavior.
These fixed budgets are provisional local checks, not cross-machine promises.

For QML traces, use a separate build with `QT_QML_DEBUG` and qmlprofiler. Do not
compare its CPU directly against a normal competitor Release build, or infer
native CPU hotspots by subtracting QML trace durations from process CPU time.

## Initial investigation: 2026-10-07

The working tree already contained uncommitted performance changes. They were
preserved; the initial measurement is not a clean Git HEAD baseline.

Synthetic input: 32 rotating topics, 2,048-byte JSON, one message every 10 ms
(nominal 100 messages/s). No MQTT broker/network traffic is exercised. Native
sampling found text layout, delegate reuse/creation, scene graph rendering, and
background workers; most sampled thread time was waiting. A waiting stack is
not CPU consumption, and sample counts are not measured CPU percentages.

Initial exploratory windows excluded the first five seconds, then measured
about 11 seconds using process CPU-time deltas while taking a five-second
native sample. These short, single-run windows are diagnostic, not release
acceptance measurements:

| Build / change | Steady CPU | Result against provisional 20% budget |
| --- | ---: | --- |
| RelWithDebInfo + QML debugging, existing working tree | 25.00% | Fail |
| Same build, visible batches changed from 16 to 33 ms | 22.27% | Fail |
| Same build, lazy row actions and explicit text width hints | 17.27% | Pass |
| Release, profiling enabled without QML debugging, precise event roles | 16.82% | Pass |

These exploratory results were not sustained in subsequent, longer tests and
must not be presented as an accepted CPU improvement. Release idle after a
single synthetic message and recovery measured 0.64% against a provisional 3%
budget. The reported high idle CPU was not reproduced in this isolated state.

Subsequent Release runs used five-second warmup and fifteen-second process
CPU-time windows, without a native sampler attached. These were single runs,
not a randomized, repeated A/B comparison. Every completed run in this table
retained 2,000 messages at approximately 100 messages/s with zero drops and
zero pressure-skipped parses:

| Stage | Mode | CPU | Local budget |
| --- | --- | ---: | --- |
| Batching, lazy controls and precise event roles | Live | 30.27% | Fail |
| Follow-scroll request deduplication | Live | 26.53% | Fail |
| Direct-source-model experiment (subsequently reverted) | Live | 25.87% | Fail |
| Before precise connection-row notifications | Paused | 16.62% | Pass |
| After precise connection-row notifications | Paused | 20.67% | Fail |

The connection-row fix is verified by notification-count regression tests,
not by an observed CPU reduction. A later fifteen-second run with the final
application code retained 2,500 messages and measured 34.85% (fail). The first
thirty-second run measured 29.13% (fail), but no recovery summary arrived
within its thirty-second recovery timeout; its integrity was not established.
The runner's recovery timeout was subsequently extended to sixty seconds.
These failures remain evidence against release acceptance; do not select the
earlier 16.82% window as the final result.

Progress logging then exposed input-rate drift: in a paused 4,000-message run,
the first 2,000 messages took 20.016 seconds but reaching 3,000 took 50.522
seconds. That run measured 15.92% CPU but never produced a recovery summary
before timeout. It is not a valid fixed-rate performance result. This was
consistent with background App Nap throttling; the process-local override
restored approximately 100 messages/s and completed all 4,000 messages.

With the override and the final application code, the first thirty-second
live run measured 33.89% CPU (fail), retained all 4,000 messages at 100.02
messages/s, recovered to normal, and reported zero drops or skipped parsing.
Longer, controlled results still do not meet the provisional 20% budget.

Repeated controlled runs of the same presentation code:

| Mode | CPU | Actual input rate | Accounted messages | Recovery |
| --- | ---: | ---: | ---: | --- |
| Live 1 | 33.89% | 100.02/s | 4,000 | Normal, no drops / skipped parses |
| Live 2 | 33.93% | 100.00/s | 4,000 | Normal, no drops / skipped parses |
| Live 3 | 34.46% | 100.00/s | 4,000 | Normal, no drops / skipped parses |
| Paused (single run) | 18.45% | 100.03/s | 4,000 | Normal, no drops / skipped parses |

Live median: 33.93%. Pausing output changed the result substantially under
these controlled conditions, supporting further investigation of message-list
presentation; it does not isolate an individual binding or rendering stage.
The above summaries checked captured counts and pipeline counters, not a
separate database read. The driver and runner were subsequently strengthened
to verify persisted message count and successful JSON parse count against the
expected workload after recovery, outside the CPU measurement window.
The strengthened runner measured 35.05% CPU at 99.94 messages/s and confirmed
4,000 persisted messages and 4,000 successful JSON parses, with zero drops or
skipped parses and normal recovery. This validation run also failed the CPU
budget. Across all four controlled live runs, the CPU median is 34.20%.
A final thirty-second idle run with the same process-local override measured
0.80% (pass against the provisional 3% idle budget). Its single message was
confirmed persisted and successfully parsed. It was disconnected idle, not
the user's connected-idle reproduction.

Changes only affect presentation work: visible rows are coalesced over 33 ms
instead of 16 ms; messages are still admitted, parsed, and stored normally.
The UI can reflect a message up to one batching interval later, in addition to
the existing follow-scroll delay. Row action controls are created only while
shown, retaining the original 76-pixel action width. Text width hints avoid
using long payloads to negotiate preferred row widths. Model updates announce
only roles whose values actually changed, including non-parser metadata.
Connection rows also notify only changed fields and affected rows; traffic
already viewed by the user no longer invalidates every connection role.
Follow-scroll requests remain queued through layout and positioning so the
layout's own height notification does not schedule a duplicate follow pass.

Validation: Debug and Release builds; all 42 Qt Test executables; application
QML lint; exact-role regression test observed failing (8 roles instead of 1)
before the fix and passing afterwards. No broker-based competitor comparison
or manual scrolling/hover acceptance has been completed. Screenshot permission
preflight was blocked by the safety reviewer because it could request a
persistent Screen Recording permission; no screenshot was captured.

Local native samples and raw logs remain temporary and untracked. Personal
filesystem paths from raw evidence are deliberately omitted here. This report
contains only synthetic inputs and aggregate values, not credentials or private
session data.

Release promotion remains gated on the full same-broker competitor matrix and
the unreproduced idle state. The provisional budgets are not evidence that the
reported MQTTX / MQTT Explorer gap has been eliminated.

Reference: [Qt Quick performance guidance](https://doc.qt.io/qt-6/qtquick-performance.html).
See also Apple's [activity and App Nap guidance](https://developer.apple.com/library/archive/documentation/Performance/Conceptual/power_efficiency_guidelines_osx/PrioritizeWorkAtTheAppLevel.html)
and [volatile command-line preference domain](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/UserDefaults/AboutPreferenceDomains/AboutPreferenceDomains.html).

## Follow-up: 2026-10-08

The footer's display count was refreshed every 50 ms, independently of the
message list. This historical experiment changed its interval to 250 ms;
the final policy below retains that interval only while output is paused.
The authoritative total still updates
immediately and messages are still admitted, parsed and persisted unchanged.
Only the footer number can lag by approximately one display interval after the
existing activity notification. Clearing history and switching sessions still
synchronize the footer immediately. This deliberately reduces unnecessary
status-counter redraws without changing the configured message-list follow rate.
Manual scrolling remains unverified.

`limitsDisplayCountRefreshesDuringSteadyTraffic` failed before the change
(9 footer refreshes in 695 ms), then passed after it. The test also checks exact
count notifications and eventual convergence to the final value. Existing
clear / session-switch tests remain passing.

With the same Release workload and thirty-second CPU window:

| Change | Mode | CPU | Persistence / parsing |
| --- | --- | ---: | --- |
| Footer display interval 250 ms | Paused | 12.82% | All 4,000 stored and successfully parsed |
| Additional lazy pressure-popover experiment | Paused | 12.82% | All 4,000 stored and successfully parsed |
| Footer tuning only | Live | 32.90% | All 4,000 stored and successfully parsed |
| Additional zero follow-cache experiment | Live | 38.06% | All 4,000 stored and successfully parsed |

The popover experiment gave no measured benefit and was reverted. Removing
the follow-mode cache worsened CPU and was also reverted. These are diagnostic
single runs, not a repeated A/B acceptance result; the counter change's first
paused measurement is 5.63 percentage points below the earlier single paused
baseline, but the live result still fails the provisional budget.

A separate native sample began twenty seconds after launch, with the 1,200-row
window already saturated. It found follow-layout, delegate reuse, role-to-QML
property updates, garbage collection and text shaping. Its sampler-attached
short window measured 21.60%; that different warmup/window is not substituted
for the unattended benchmark result. Qt's local container source also showed
front erasure advances the data pointer rather than moving every remaining
row, so the proposed full-window movement hypothesis was rejected without
adding type declarations or changing storage.

### Repeated footer A/B validation

Two Release application snapshots differed only in the footer display-count
interval (50 vs 250 ms). Source and the main Release artifact were restored to
250 ms before running the comparison. The main artifact's SHA-256 matched the
250 ms snapshot (`c5b80b2f6df7b88206edc5b9c05055419c080e134be117d7dc64349a244cfd5e`);
the 50 ms reference hash was
`bb7b5ed50f50070dee7b927653cf120dd8415d6e29d3c0508d22855bbc2d6bab`.
Both snapshots used the same isolated test data and process-local App Nap
override, sequentially, in AB / BA / AB order. Each measured thirty seconds
after five seconds of warmup, with output paused and 100 messages/s of 2 KB
JSON input. All results are retained:

| Run order | Footer interval | CPU | Actual input rate |
| --- | ---: | ---: | ---: |
| 1 | 50 ms | 20.15% | 100.02/s |
| 2 | 250 ms | 13.15% | 99.99/s |
| 3 | 250 ms | 13.12% | 100.02/s |
| 4 | 50 ms | 20.68% | 99.96/s |
| 5 | 50 ms | 19.67% | 100.02/s |
| 6 | 250 ms | 13.12% | 100.02/s |

Medians: 20.15% vs 13.12%, a decrease of 7.03 percentage points (34.89%
relative) in this paused-output workload. Every run independently confirmed
4,000 persisted messages and 4,000 successful parses, zero drops / skipped
parses, and normal recovery. This is evidence for the footer optimization,
not evidence that the live, scrolling, connected-idle or competitor gates have
passed. Final Debug and Release builds, all 42 test executables, QML lint and
diff whitespace checks passed; the added footer-rate test was observed failing
before the change and passing after it.

### Frame rate and varying-payload probes

The explicit profiling build now counts actual window frame swaps using a
queued connection. Normal builds do not install this callback. Frame counts
cover the complete injection and recovery interval, not exactly the CPU
measurement window. The constant-payload live run measured 32.53% CPU and
28.70 frames/s over 40.83 seconds. This does not support an excessive frame
frequency hypothesis, but does not rule out expensive work within each frame.

The benchmark runner also accepts `live-varied` and `paused-varied`. These
keep each JSON payload at 2 KB but include a unique sequence number. The
runner checks that every sequence survives persistence and successful parsing,
so repeated identical text cannot conceal missing or stale messages.

| Probe | CPU | Frames/s | Unique sequences stored / parsed |
| --- | ---: | ---: | ---: |
| Varying payload baseline | 29.41% | 29.27 | 4,000 / 4,000 |
| Delayed payload binding | 30.56% | 34.16 | 4,000 / 4,000 |
| Removing seven unused delegate role bindings | 29.90% | 29.32 | 4,000 / 4,000 |

Each used a thirty-second CPU window, five-second warmup and approximately
100 messages/s, with zero drops or skipped parses and normal recovery. These
are single-run diagnostic results, not repeated A/B evidence. Both experiments
were reverted because neither demonstrated an improvement. The varying-payload
baseline cannot be directly compared with the constant-payload measurements to
claim a gain. Live CPU still fails the provisional 20% budget.

After reverting both probes, the Debug build, all 42 test executables
(31.27 seconds), QML lint and shell syntax checks passed. The retained frame
counter and sequence verification are confined to explicit synthetic profiling.

### Delegate cost probes

Three further one-variable probes used the same varying-payload live workload:

| Probe | CPU | Frames/s | Outcome |
| --- | ---: | ---: | --- |
| Replace payload Label with Text | 28.81% | 29.79 | Inconclusive small change; reverted |
| Reserve 100 px minimum metadata width instead of 76 px | 29.63% | 29.42 | No improvement; reverted |
| Disable ListView delegate reuse | 38.23% | 27.40 | Invalid rate: 93.11/s; reverted |

The reference varying-payload measurement was 29.41%. These diagnostic single
runs are not repeated A/B acceptance evidence. The first two do not justify
retaining UI changes. The reuse-disabled run failed the 95-105 messages/s rate
gate, so its CPU cannot be used as a valid same-rate comparison. It provides
no evidence for disabling reuse, which was reverted. Recovery summaries still
independently check 4,000 unique
persisted and successfully parsed sequences, zero drops and normal recovery.
The original Label, metadata minimum and delegate reuse remain in place.
After restoring the original UI, Debug and Release builds and QML lint passed;
all 42 test executables passed in 21.33 seconds. No new UI optimization is
claimed from these probes.

### Native thread CPU attribution

On macOS, explicit synthetic profiling now takes two native thread CPU
snapshots, before injection and before persistence verification. It uses
`THREAD_BASIC_INFO` user/system CPU time, not instantaneous CPU utilization or
sample-stack frequencies. Native thread identities match the snapshots; thread
ports and the returned array are released. Worker threads have descriptive
names; naming does not change their scheduling or workload. Other platforms
report thread attribution as unavailable. Normal application runs do not take
these snapshots.

See Apple's [thread CPU accounting implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/kern/bsd_kern.c)
and [recount observability documentation](https://github.com/apple-oss-distributions/xnu/blob/main/doc/observability/recount.md).

Threads that exit before the final snapshot are not included in the observed
sum. `exitedObservedThreads` reports this limitation; `processCpuMs` independently
measures total process CPU using `getrusage`, and `cpuAccountingDifferenceMs`
reports the signed difference from the observed thread sum. Snapshot calls are
not simultaneous, so small accounting differences are also possible. Thread
percentages use the full injection/recovery interval, not the runner's separate
thirty-second CPU window.

The final named-thread live-varied diagnostic used 4,000 unique 2 KB messages
at 99.68/s. The CPU gate still failed at 29.34%. During the 40.682-second
injection/recovery interval, the process used 11,704.803 ms CPU; observed threads
accounted for 11,554.376 ms (98.71%). Twelve initially observed threads exited;
the accounting difference was 150.427 ms.

| Thread | CPU milliseconds | Percent of one core during observation |
| --- | ---: | ---: |
| Main | 8,511.424 | 20.92% |
| QSGRenderThread | 1,425.436 | 3.50% |
| MessageHistoryWriter | 561.729 | 1.38% |
| MessageAdmission | 285.328 | 0.70% |
| MessageParser | 224.468 | 0.55% |

An earlier live-varied run measured 29.60% at the CPU gate and 21.16% main-thread
CPU over its complete observation. A paused-varied diagnostic measured 13.71%
at the gate, 5.04% main-thread CPU and 1.37% render-thread CPU over its own
complete interval. All three independently verified 4,000 persisted unique
sequences, successful parsing, zero drops / skips and normal recovery. These
are diagnostic runs, not repeated competitor A/B acceptance.

In the named-thread live run, the main thread accounted for 72.72% of measured
process CPU. This prioritizes main-thread model/delegate/layout work for the
next investigation; it does not prove one particular binding is responsible
or justify redesigning parser/storage workers first. The connected-idle,
manual-scroll and same-broker competitor requirements remain unverified.
The final Debug and Release builds passed, QML lint passed, and all 42 test
executables passed in 30.75 seconds after adding attribution and worker names.

### Follow-tail correctness gate and layout probes

Live benchmark modes now check the actual rendered list at recovery. The
last delegate must exist, be visible, have its bottom inside the viewport,
and match the newest persisted message's history ID. Merely retaining every
message or having `atYEnd` set is not sufficient. Paused modes do not require
this gate. The list's object name is diagnostic only; normal runs do not perform
the window/delegate inspection.

The new gate was exercised with a deliberately broken temporary build that
positioned the list at its beginning instead of its end. A short negative
control used 1,200 unique messages, 100.08/s and a deliberately loose CPU
budget. CPU, persistence, sequence and parsing checks passed, but the gate
returned `followTailChecked: true`, `followTailVisible: false` and exit 1.
The incorrect positioning was then restored. This is a verifier test, not a
short-window CPU acceptance measurement.

Two single-variable layout probes were also tested at 4,000 messages, with the
full thirty-second CPU window and the final delegate visible at the viewport
bottom in each run:

| Probe | CPU | Main-thread CPU over complete observation | Outcome |
| --- | ---: | ---: | --- |
| Original layout with follow-tail check | 29.87% | 21.59% | Reference |
| Remove explicit forceLayout before positioning | 29.77% | 21.31% | No demonstrated benefit; reverted |
| Replace outer row/column layouts with anchors/Column | 29.73% | 21.64% | No demonstrated benefit; reverted |

Each run persisted and successfully parsed all 4,000 unique sequences, had no
drops / skipped parses, recovered normally and stayed within the input-rate
gate. These are diagnostic single runs, not repeated A/B acceptance evidence.
The explicit layout call and original nested layouts remain in place.
Qt 6.11's [item-view source](https://raw.githubusercontent.com/qt/qtdeclarative/6.11/src/quick/items/qquickitemview.cpp)
shows that positioning applies pending model changes; that fact alone did not
translate into measured improvement here, so it is not used to justify a change.

After restoring the original UI, the stronger gate also matched the newest
persisted message ID, not just the final model row. The thirty-second live-varied
run measured 29.68% CPU at 99.42 messages/s; all 4,000 unique messages were stored
and successfully parsed, with zero drops / skips and normal recovery. The tail
bottom and viewport height both measured 691 px. The CPU gate still failed.
Final Debug and Release builds, QML lint, shell syntax and whitespace checks
passed; all 42 test executables passed in 22.25 seconds.

### Native text-layout attribution

An own-process macOS Time Profiler capture identified text shaping during
delegate reuse as a main-thread hotspot. The first attached capture collected
mostly idle samples and was rejected. A second capture launched only the
isolated synthetic application and captured message traffic. Its 35-second
recording reached the time limit and stopped the launched target (SIGKILL);
the recorder returned 54. This is diagnostic evidence, not a successful CPU
acceptance run or evidence of an application crash. All 2,500 unique messages
were independently verified as stored and parsed before the tool stopped it.

In the selected 15-26 second traffic interval, 1,201 of 1,913 weighted samples
were on the main thread. Inclusive main-thread samples included 770 in
delegate reuse, 631 in text assignment, 505 in text shaping and 473 in
HarfBuzz shaping. These stacks overlap: their percentages must not be added.
Of 142 main-thread memset leaf samples, 127 occurred under CoreText glyph
advance calculation and text shaping. This prioritizes the payload text path
rather than attributing those samples to unrelated layout geometry or storage.
Instrumentation and launch conditions affect frame cadence, so this trace's
CPU/frame numbers are not compared with the uninstrumented benchmark.

Raw traces and exported metadata remain in temporary, untracked storage;
environment variables, machine identifiers and personal paths are omitted
from this report. Only aggregate stack names and synthetic data are reported.

### Historical ASCII payload shaping A/B (Reverted)

This subsection records the preliminary decision before the settings-isolation
audit. The corrected isolated comparison below supersedes it: this change is
no longer retained in the application.

The payload label now disables optional shaping only when its displayed text
is entirely ASCII. Any non-ASCII code unit keeps the original shaping path,
including Chinese, Arabic, Devanagari, combining marks, emoji and mixed JSON.
This is restricted to payload text, not the topic, editor or inspector.
The [Qt font documentation](https://doc.qt.io/qt-6/qml-font.html) describes
`preferShaping`; skipping optional shaping can change cosmetic Latin ligatures
or spacing. Message content, copy data, expansion and line limits are unchanged.
Visual typography equivalence has not been established with screenshots.

An architecture regression test was observed failing before the change and
passing afterward. It evaluates the actual QML guard expression with QJSEngine
over ASCII and non-ASCII cases, including a transition back to ASCII during
delegate reuse. This proves the classification, not glyph appearance.

Two Release snapshots differing only in the payload shaping guard and font
property were tested sequentially in AB / BA / AB order. Each run used a
five-second warmup followed by a thirty-second process CPU window, no profiler,
4,000 unique 2 KB JSON payloads and the same 100/s target rate.

| Execution order | Original CPU | ASCII-only fast path CPU |
| --- | ---: | ---: |
| AB | 28.98% | 27.77% |
| BA | 28.81% | 26.57% |
| AB | 28.37% | 26.78% |
| Median | 28.81% | 26.78% |

The median reduction is 2.03 percentage points, or 7.05% relative. Each paired
fast-path run was lower. All six runs independently verified every persisted
sequence and successful parse, zero drops / skips, normal recovery and the
newest persisted message visibly at the bottom. Actual injection rates ranged
from 99.81 to 100.02/s. Whole-observation rendered frame rates were 51.76-52.68
for original and 52.53-53.31 for the fast path; no frame-rate cap was reduced.
Main-thread CPU medians over the separate whole observation were 18.90% and
15.57%; these are not the thirty-second process CPU values above.

Snapshot executable SHA-256 values:

- Original: `8a82c6d856ac13fe8fc2a6eb5765a0336eb130e8b96f39601fb6043a530ce8f3`
- ASCII fast path: `c73f273994c9d4a08beb5012e6119299fac40971a7dabd20ed5429c53de5ea08`

The change is retained based on this repeated synthetic-workload result.
All six CPU budget gates still failed the provisional 20% limit. This is not
a same-broker competitor result, a connected-idle result or a manual-scroll
result. It does not establish the user's overall performance requirement.

After retaining the guard, Debug and Release builds passed, QML lint passed,
and all 42 test executables passed in 28.60 seconds. Shell syntax and whitespace
checks also passed. Connected-idle, manual-scroll, typography screenshots and
same-broker competitor acceptance remain outstanding.

### Rejected additional kerning probe

A subsequent one-property experiment also disabled kerning for ASCII payloads,
using the same content guard. Qt's [font documentation](https://doc.qt.io/qt-6/qml-font.html)
notes a possible performance/cosmetic tradeoff. This was tested against the
retained ASCII shaping optimization, not against the original pre-optimization
application, using separate Release snapshots in AB / BA order.

| Execution order | Retained shaping path CPU | Additional kerning change CPU |
| --- | ---: | ---: |
| AB | 32.19% | 31.39% |
| BA | 31.66% | 31.67% |

All four runs verified 4,000 unique persisted/parsed messages, zero drops /
skips, normal recovery and the newest message at the viewport bottom. Input
rates were 99.53-99.81/s. Main-thread CPU over the complete observation was
18.05%, 17.65%, 18.08% and 18.11% in execution order. The second pair showed
essentially no process or main-thread improvement, so the additional kerning
property was removed. The earlier ASCII shaping guard remains unchanged.

These runs had whole-observation frame rates of 29.39-29.99/s, unlike the
51.76-53.31/s observed in the preceding shaping A/B. Absolute CPU values from
the two measurement epochs are not used to claim a regression or improvement.
The kerning probe did not establish a reliable additional gain and all four
runs still failed the provisional 20% process CPU budget.

After removing the experimental property, Debug and Release builds passed,
QML lint passed and all 42 test executables passed in 16.28 seconds. The
retained source is unchanged by this probe; only the evidence above is added.

### Explicit Settings Isolation And Connected Idle

The macOS isolation audit found that the previous organization/application
QSettings constructor still used NativeFormat. Qt test-mode data paths alone
did not isolate this native preference store. Qt's [QSettings documentation](https://doc.qt.io/qt-6/qsettings.html)
distinguishes the explicit format constructor from the organization constructor;
`setDefaultFormat` does not change the latter. The earlier isolation assertion
only checked startup ordering, so it was insufficient. Earlier numeric results
are historical diagnostic evidence, not strictly isolated acceptance evidence.
Native preferences are not cleared or guessed back to an unverified state.

Explicit profiling now creates a fresh temporary directory, selects IniFormat
for Application, redirects both user and system INI paths, and disables update
checks before composing services. Normal application runs still select
NativeFormat. The fixture explicitly selects the subscriptions pane so the
message list is visible rather than inheriting a user's workspace layout.
The runner checks the isolation marker in the executable before launching and
again in the run log. An older snapshot was rejected with exit 2 before launch.
Architecture guards cover the explicit format, both scopes, startup ordering
and loopback configuration without inherited connection credentials.

The new `connected-idle` scenario starts its own Mosquitto broker with an
ephemeral port, a 127.0.0.1-only listener, anonymous synthetic access and no
persistence. It waits for the application's normal MQTT connection workflow
before beginning observation. A single synthetic message populates the list;
it is not published through the broker. No incoming broker traffic is generated.
The run observes at least forty seconds with a thirty-second CPU window after
five seconds of warmup. Acceptance requires an uninterrupted connection,
broker-confirmed PINGREQ/PINGRESP, the displayed fixture message, successful
persistence/parsing and normal recovery. Short connected-idle CPU windows are
rejected. Cleanup signals only children still owned by the runner.

```bash
bash scripts/benchmark-cpu-macos.sh \
  build/cpu-release/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app connected-idle 3
```

The explicit low-level options are `--profile-local-broker-port` (1-65535,
fixed loopback host) and `--profile-observation-ms` (1-600000). Invalid port 0,
port 65536 and observation 600001 were each observed exiting 2 before composing
Application. These options require the existing explicit profiling workload
flag and a profiling-enabled build.

With fresh INI settings and the visible message list, the first connected-idle
run measured 1.36% CPU, and the disconnected control measured 0.83%. Each
observed more than forty seconds, independently verified the one fixture
message, and displayed it in a 594 px-high viewport. The connected run kept its
connection and completed a broker-confirmed keepalive round trip. These are
single diagnostic measurements, not competitor acceptance or before/after
optimization evidence.

A second visible-list connected-idle run after the child-ownership cleanup
check also measured 1.36% CPU over thirty seconds, observed 40.889 seconds,
kept its connection and completed the keepalive gate. The fixture message
remained visible, persisted and successfully parsed. This repeats the local
connected-idle observation; it is still not a competitor comparison.

Two earlier fresh-INI connected-idle probes measured 1.30% and 1.23%, and their
disconnected control measured 0.63%. Their default topics pane hid the message
list (viewport height zero); they are not used as visible-message-list evidence.
The fixture was corrected and idle modes now also enforce the visibility gate.
A preliminary 1.23% local connection measurement before the INI correction is
excluded from strictly isolated evidence.

The connection gate was tested by stopping only the runner's own broker child
during observation. The CPU gate passed a deliberately loose budget, the one
message still persisted and parsed, and pressure recovered normally, but
`brokerConnected` and `brokerStayedConnected` were false. The runner correctly
returned 1 for the failed connection check. Tests/lint overlapped that negative
control; its CPU is not acceptance evidence. Final Debug and Release builds,
QML lint and all 42 test executables passed (16.54 seconds); shell syntax and
whitespace checks passed.

Same-broker competitor comparison, live broker traffic, manual scrolling,
typography screenshots and reassessment of retained optimizations under the
corrected settings fixture remain outstanding. The goal is not complete.

### Corrected Isolated Shaping Comparison

The earlier ASCII shaping optimization was reassessed with fresh temporary
INI settings, disabled update checks and an explicitly visible subscriptions
message list. Separate Release snapshots differed only in the payload shaping
guard and `font.preferShaping` property. Six sequential runs used AB / BA / AB
order, the same five-second warmup, thirty-second CPU window, 4,000 unique
2 KB JSON payloads and a 100/s target injection rate. No profiler or concurrent
build/test workload ran during these CPU measurements.

| Execution order | Default shaping CPU | ASCII optimization CPU |
| --- | ---: | ---: |
| AB | 29.70% | 32.39% |
| BA | 29.36% | 32.10% |
| AB | 30.03% | 32.68% |
| Median | 29.70% | 32.39% |

Every paired optimized run was worse for total process CPU. The median
increase was 2.69 percentage points (9.06% relative). All six runs verified
4,000 unique persisted messages and successful parses, zero drops / skipped
parses, normal recovery and the newest persisted message visible in a
594 px-high viewport. Injection rates were 99.13-99.99/s. Whole-observation
rendered frame rates were 28.91-29.53/s for default shaping and 28.87-29.36/s
for the ASCII path, without changing the frame-rate cap.

Main-thread CPU medians over the separate whole observation did decrease,
from 21.43% to 16.45%, but render-thread medians increased from 3.12% to 7.52%,
and the other observed threads collectively also used more CPU. This does not
identify the exact underlying Qt font/cache mechanism; it does establish that
a main-thread-only improvement was insufficient and misleading for this goal.
Whole-observation thread percentages are not summed into the thirty-second
process CPU results.

Snapshot executable SHA-256 values:

- Default shaping: `008b78639e0225b06bb2c50989e64293793856d8f96e37974dfdeefd39139aea`
- ASCII optimization: `7e5b26235cee138c844114856c38118e57cc47ffa4a9b63d77a8b63f5aac78a6`

The two QML shaping lines and their guard-specific test were removed. Payload
labels now use the original Qt shaping defaults for every script. Earlier
claims that the ASCII change should remain are retracted in favor of this
corrected evidence. All six process CPU gates still failed the provisional
20% budget; this is not same-broker competitor acceptance. Other retained
optimizations still need reassessment under the corrected settings fixture.

After restoring default shaping, Debug and Release builds passed, QML lint
passed and all 42 test executables passed in 17.24 seconds. Whitespace checks
passed. No application instances from the six-run comparison were left running.

### Real Local MQTT Traffic

The profiling driver now supports `--profile-broker-input`, which requires an
explicit loopback broker port. It waits for the normal `profile/#` subscription
to reach Subscribed before announcing readiness. In this mode it observes the
application's captured message count; it never calls the synthetic incoming
queue. The legacy `sentMessages` diagnostic field represents the observed
capture count for broker-input runs. The input-rate observation starts at the
first polling sample with messages, so it is approximate rather than the
publisher's exact pacing clock.

Two runner modes use this path:

```bash
bash scripts/benchmark-cpu-macos.sh \
  build/cpu-release/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app broker-live-varied 20
bash scripts/benchmark-cpu-macos.sh \
  build/cpu-release/mqtt_plus_app.app/Contents/MacOS/mqtt_plus_app broker-paused-varied 20
```

The runner owns a loopback-only temporary Mosquitto broker, a separate
mosquitto_pub MQTT v5 / QoS 0 process and a Perl JSON::PP fixture generator.
The generator sends unique ten-digit sequence strings and exactly 2,048-byte
JSON payloads through a FIFO, using absolute monotonic deadlines at 100/s.
The publisher uses an empty temporary HOME/config directory and an explicit
empty options file, not the user's publisher configuration. Generator,
publisher and broker CPU are excluded from the application's process CPU.
Successful generator/publisher exits, application connection continuity,
unique persisted sequences, parsing, pressure recovery and live follow-tail
visibility are all required. Cleanup checks ownership of all four children.

The first wire-path diagnostic results were:

| Scenario | Thirty-second process CPU | Observed input rate | Stored / parsed / unique sequences |
| --- | ---: | ---: | ---: |
| Live follow | 27.30% | 100.48/s | 4,000 / 4,000 / 4,000 |
| Paused output | 15.25% | 100.25/s | 4,000 / 4,000 / 4,000 |

Both generators completed 4,000 payloads in 39.993 seconds. Both broker logs
independently recorded 4,000 received publishes; the live log also confirmed
4,000 sends to the application. Both runs had zero drops / skipped parses,
normal recovery and an uninterrupted subscriber connection. Live follow
showed the newest persisted record at the bottom of a 594 px viewport.
Paused output deliberately had no visible message rows, while receiving,
persisting and parsing every message. The live CPU gate still failed 20%;
the paused control passed. These are single diagnostic runs, not repeated
optimization A/B evidence or competitor acceptance.

This wire fixture publishes to one topic, unlike the direct-injection fixture's
32-topic rotation. CPU values from those different workloads are not compared
as an improvement. Production-rate / payload / QoS matching and same-broker
competitor comparison still require the user's actual comparison parameters.
Manual scrolling and reassessment of retained optimizations also remain open.

The JSON generator's exact sizes and sequence values were checked on a small
fixture, and count zero was rejected. Broker-input mode without a local port
was observed exiting 2 before composing Application. An architecture guard
checks that the broker observation branch uses captured counts without calling
the direct incoming queue; runtime wire/persistence checks provide the stronger
end-to-end evidence. Normal non-profiling application behavior is unchanged.

Debug and Release builds, QML lint and all 42 test executables passed after
adding the wire fixture (17.54 seconds for the suite). Bash/Perl syntax and
whitespace checks passed. No CPU acceptance measurements overlapped those
builds or tests.

A short direct-injection regression after the driver refactor verified all
1,100 unique messages, successful parsing, zero drops / skips, normal recovery
and visible follow-tail with `brokerInput: false`. It used a deliberately loose
CPU budget and a one-second CPU window; only correctness, not performance
acceptance, is established by that check.

### Corrected Wire Footer Refresh A/B

The display-count refresh change was reassessed using fresh temporary INI
settings and real loopback MQTT traffic. Two Release snapshots differed only
in the display counter's timer interval (50 ms reference versus
250 ms). Authoritative capture counts, parser/storage behavior and the message
follow-rate setting were unchanged. Six sequential paused-output runs used
AB / BA / AB order, a five-second warmup and thirty-second process CPU window,
with 4,000 unique 2 KB payloads at a 100/s target rate. No builds/tests overlapped
these measurements.

| Execution order | 50 ms display refresh CPU | 250 ms display refresh CPU |
| --- | ---: | ---: |
| AB | 15.65% | 12.02% |
| BA | 17.32% | 10.46% |
| AB | 17.88% | 11.72% |
| Median | 17.32% | 11.72% |

Every paired 250 ms run used less total process CPU. The median reduction
was 5.60 percentage points (32.33% relative). All six runs kept their subscriber
connection, independently verified 4,000 persisted / parsed / unique messages,
had zero drops / skipped parses and recovered normally. Observed input rates
were 100.25-100.37/s. Paused output intentionally had no visible message rows;
this is not a live-follow or manual-scroll result.

Whole-observation frame rates were 21.14-23.45/s for the reference and
5.78-6.02/s for the retained interval: fewer display-count updates avoided
unnecessary redraws while the output list was paused. This is an intentional
bounded presentation-rate change, not reduced message acquisition or discarded
data. Tests cover display-rate bounds, exact authoritative counts, eventual
display convergence and immediate clearing/session switching.

Snapshot executable SHA-256 values:

- 50 ms: `83ae693e1054f567ad6b5c04cee8824add586394f5b62e8ee09c8e0dead522c7`
- 250 ms: `f8752c6f11725c28f912e746f78397d6d5ee66469ac37c0d9cce5a735b725fff`

The asynchronous result cell was unavailable after interruption, but all six
completed command results and integrity summaries were recovered from stored
tool state. Each runner had terminated successfully, and a subsequent process
check found no test application remaining. The source and current Release
build had already been restored to 250 ms before measurements. The overall
same-broker competitor requirement is not proven by this paused control.

The same snapshots were then compared during live follow in six more sequential
AB / BA / AB runs, without overlapping builds or tests:

| Execution order | 50 ms display refresh CPU | 250 ms display refresh CPU |
| --- | ---: | ---: |
| AB | 24.38% | 25.04% |
| BA | 19.72% | 25.58% |
| AB | 24.74% | 25.44% |
| Median | 24.38% | 25.44% |

All six live runs independently verified 4,000 stored / parsed / unique messages,
an uninterrupted connection, zero drops / skips, normal recovery and the newest
persisted message visible in a 594 px viewport. Observed input was
100.25-100.46/s. The 19.72% reference result is retained, not discarded as an
outlier. Five runs failed the provisional 20% CPU gate; only that reference run
passed. The 250 ms median was 1.06 percentage points higher (4.35% relative),
and every paired 250 ms run used more process CPU. Whole-observation frame
cadence was 42.20-43.70/s versus 30.16-30.76/s; these are not comparisons at
matched physical frame frequency. The mechanism behind the live difference
has not been established.

Consequently the production change is scoped to paused output: a 250 ms footer
interval while paused, and the original 50 ms interval during active output.
The interval is chosen only when starting an inactive timer, so incoming
messages cannot continuously restart it. Pause/resume immediately synchronizes
the displayed count and stops pending timers through the existing session-state
notification. Regression tests check both steady-state rates and both switch
directions. The active-rate test was observed failing with constant 250 ms
before implementing the adaptive policy.

Final adaptive-policy validation used the rebuilt Release artifact with the
same isolated wire fixture, five-second warmup and thirty-second CPU window.
Paused output measured 12.42%; live follow measured 29.08% and still failed the
provisional 20% gate. These single validation runs are not paired A/B improvement
evidence and do not replace the repeated snapshot results above. Both verified
all 4,000 stored / parsed / unique messages, zero drops / skips, normal recovery
and an uninterrupted connection. Live follow showed the newest persisted row
at the bottom of a 594 px viewport. Observed rates were 100.31/s paused and
100.25/s live. The final live observation recorded 42.32 rendered frames/s.
Remaining live CPU therefore cannot be described as resolved.

Debug and Release builds, QML lint and all 42 test executables passed with the
adaptive policy (19.56 seconds for the full suite). Both steady-state cases and
pause/resume directions passed in the focused regression. No CPU validation
measurement overlapped builds/tests. Whitespace checks passed.

### Wire Input With Automated Wheel Scrolling

`broker-scroll-varied` adds an explicit profiling-only `--profile-scroll-output`
option. It sends timestamped mouse wheel angle events to the application's
own QQuickWindow at a 16 ms target interval. It does not assign `contentY` or
invoke a scrolling helper. After at least 200 rows are available, it scrolls
back and forth inside the list, avoiding the ends that trigger smart-follow
resumption or older-history paging. This exercises Qt Quick's normal input
handling. A physical user/trackpad check remains separate.

The runner rejects older binaries before launch and rejects the incompatible
paused-output + scrolling flags before composing Application. Its scroll gate
requires at least 100 events, over 90% observed in manual mode, at least 1,000 px
of manual movement and a model-matching positive-ID message delegate intersecting
the viewport. Thirty-second runs also require a direction reversal. Tail-follow
is not required or reported as checked in this mode. Production manual mode
freezes the displayed stream while incoming messages continue to be captured,
parsed and persisted; the frozen visible row count is not a capture count.

Two deliberately loose-budget one-second probes failed the scroll gate:
pixel-delta events left the list following automatically, while angle events
without timestamps entered manual mode but moved only 12 px. Movement attributed
to follow/append activity is not counted as manual movement. Adding monotonic
event timestamps produced a short passing correctness probe: approximately
11,919 px manual movement and all 1,100 messages stored / parsed / unique.
Those short or failed probes are not CPU acceptance results.

The final isolated Release baseline used real loopback MQTT input, 4,000 unique
2 KB messages, five-second warmup and a thirty-second CPU window with no builds,
tests or sampling overlapping it. Process CPU measured **45.45%**, failing the
provisional 20% gate. Input averaged 100.29/s. All 4,000 messages were stored,
successfully parsed and sequence-verified, with zero drops / skips, normal
recovery and an uninterrupted connection. The list showed a model-matching
message in its 594 px viewport. Of 2,394 events, 2,393 were observed in manual
mode; cumulative manual movement was 49,299.75 px with three reversals. The
displayed stream froze at 201 rows, as expected from production manual mode.

Whole-observation frame cadence was 116.28/s. Thread accounting over the entire
40.66-second observation attributed 15.83% of a core to the render thread and
15.44% to the main thread; these are not the same thirty-second window as the
process measurement and omit exited-thread CPU. The result establishes a
repeatable unresolved scrolling workload, not an optimization gain or a
competitor comparison. It must not be compared as a delta against live-follow
results with different frame cadence / visible-stream behavior.

Debug and Release builds, QML lint and all 42 test executables passed after
adding the fixture (17.54 seconds for the latest full suite). Architecture
guards require window-event delivery, event timestamps and no direct property
assignments in the scroll routine. The runtime movement/message gates provide
the stronger behavioral check. Bash syntax and whitespace checks passed.

A separate five-second native wall-stack sample targeted only the runner's
unique application child during scrolling. It did not terminate the application;
the diagnostic run subsequently completed all 4,000 message integrity checks.
Sampling overlapped its CPU window, so that run's loose-budget CPU result is
excluded from acceptance. Of 613 wall observations, the main thread was in
render synchronization waits in 467 and the render thread in Metal drawable
semaphore waits in 592. These waits are not on-CPU hotspots, so their large wall
sample share does not explain the measured CPU or justify changing rendering
backends. Text layout/reuse also appeared, but this sample is too sparse to
quantify its CPU share.

The sample also caught the profiling driver's repeated recursive object lookup.
The driver now caches the list in a QPointer and reacquires it only if destroyed,
avoiding that unnecessary measurement overhead. This is a fixture correction,
not a production performance gain. The rebuilt no-sampler baseline measured
48.58% over thirty seconds, still failing the 20% gate. It verified all 4,000
stored / parsed / unique messages, zero drops / skips, normal recovery and an
uninterrupted connection at 100.29/s. Of 2,315 wheel events, 2,314 were observed
in manual mode, with 49,244.26 px movement, three reversals and a model-matching
message visible in a 594 px viewport. Whole-observation cadence was 114.68/s;
the displayed stream froze at 204 rows. This is another unresolved-workload
baseline, not evidence that caching lowered or raised production CPU. The two
single no-sampler measurements do not establish a statistically controlled
delta. Builds, QML lint and all 42 tests passed after the fixture correction
(17.05 seconds for the suite).

Raw traces and runner logs remain private temporary artifacts;
the public report omits personal paths, process IDs and environment metadata.

### On-CPU Trace And Window-State Controls

An eight-second native Time Profiler recording attached to the runner's unique
application child, rather than launching or terminating it. Both recorder and
runner exited successfully; the subsequent integrity summary verified all
4,000 messages, normal recovery and uninterrupted wire input. Export selected
only the `time-profile` table. The table declares waiting-thread recording
disabled and 1 ms sample weights. A structured XML parser resolves ID references,
filters the target process and Running state, and counts unique frame names
per stack for inclusive weights. The raw trace remains private because its
metadata contains environment values and personal paths.

| Thread group | Sampled weight | Share of sampled target CPU |
| --- | ---: | ---: |
| Render thread | 301 ms | 36.13% |
| Unnamed application / platform worker threads | 256 ms | 30.73% |
| Main thread | 217 ms | 26.05% |
| History writer | 45 ms | 5.40% |
| Parser | 9 ms | 1.08% |
| Admission | 5 ms | 0.60% |

The render sample includes 155 ms in merged-batch recording, 88 ms preparing
render passes and 72 ms in Metal shader-resource binding. Main-thread text
layout accounts for 75 ms inclusive, with 62 ms in text shaping. These nested
weights must not be added together. Platform worker samples include Metal
submission and presentation callbacks. This prioritizes rendering/batching
and text reuse over MQTT parsing for this scrolling workload, but does not
establish an optimization or an energy-consumption result.

The traced run measured only 10.83% process CPU with approximately 116.86
frames/s and 53,204.58 px manual movement. That surprising value is excluded
from acceptance because tracing overlapped the measurement. It cannot be
reported as an improvement over the 41-49% untraced samples. Recording may alter
scheduling or other execution conditions; the exact mechanism is unproven.

Passive window-state instrumentation in a fresh untraced run measured 41.81%
CPU, verified all messages and recorded 116.30 frames/s. The window was active
for only 20 injection samples and inactive for 784, initially active but finally
inactive. Previous samples without this instrumentation therefore do not prove
a controlled foreground comparison.

The profiling-only `--profile-activate-window` option requests activation once,
three seconds into observation; it never repeatedly steals focus. The runner
opts in with `MQTT_PROFILE_ACTIVATE_WINDOW=1`, rejects unsupported binaries and
invalid environment values before launch, and requires active samples after
the five-second warmup, no inactive samples or activation-loss signal after
warmup, and an active final window. Recovery sampling covers idle modes too.
Normal application startup and benchmark default behavior remain unchanged.

The first opted-in control measured 46.65% but failed that foreground validity
gate: all 719 samples after warmup were inactive. It nevertheless verified
4,000 stored / parsed / unique messages, normal recovery, no drops / skips,
continuous connection and actual scrolling. The activation request alone does
not prove activation on this desktop. The CPU number is excluded from foreground
acceptance, not silently relabeled as a foreground result. Same-state competitor
comparison and physical user scrolling remain open.

Debug and Release builds, QML lint and all 42 tests passed after adding the
window-state controls (17.30 seconds for the suite); no measurement overlapped
builds or tests. Architecture guards require explicit activation opt-in and a
single activation request. Bash syntax and whitespace checks passed.

No blanket layer cache or rendering-backend switch was retained from the trace.
[Qt's layer performance guidance](https://doc.qt.io/qt-6/qml-qtquick-item.html#memory-and-performance)
warns that offscreen layers add memory/compositing costs and prevent normal
batching. Any such experiment needs measured evidence, not an assumption that
turning the interface into textures is automatically cheaper.

### Rejected Rendering Probes And Fixed Background State

A payload-only `Text.NativeRendering` probe preserved text, font, line limits,
wrapping and input rate. Two Release snapshots were run sequentially without
sampling or concurrent builds/tests. The reference measured 51.37% at 116.51
frames/s with an active window throughout observation; the native-text snapshot
measured 46.95% at 108.74 frames/s, but lost activation during observation
(128 active and 587 inactive samples after warmup). Both verified all 4,000
messages, actual scrolling and normal recovery. The comparison is not evidence
of an improvement: window state differed and frame cadence decreased. The
source and current Release artifact were restored to the default text renderer
before subsequent tests. No native-text UI change was retained; visual approval
and screenshots for a retained rendering change therefore remain unestablished.
[Qt documents the native rendering mode](https://doc.qt.io/qt-6/qml-qtquick-text.html#renderType-prop)
as platform-specific and cautions about transformed text; it is not an automatic
performance fix.

`--profile-background-window` fixes background-window testing by setting
`Qt::WindowDoesNotAcceptFocus` on the profiling window before showing it.
This requires an explicit profile request and never affects normal startup.
It is mutually exclusive with foreground activation. The runner opts in using
`MQTT_PROFILE_BACKGROUND_WINDOW=1`, rejects unsupported binaries before launch,
and requires the actual window flag, inactive samples after warmup, zero active
samples after warmup and an inactive final window. Window-control environment
values must be 0 or 1. A contradictory foreground/background request was
observed exiting 2 before composing Application. An initial sandboxed native GUI
launch could not initialize platform services; its failure is not an application
regression or a benchmark result. The native permitted run performed the actual
argument-rejection check.

With that control, two runs of the same Release binary compared explicit Qt
render-loop environment values. Neither environment value is persisted or
selected by production startup:

| Render loop | Thirty-second CPU | Whole-observation frames/s |
| --- | ---: | ---: |
| `threaded` | 47.35% | 116.18 |
| `basic` | 38.20% | 78.45 |

Both stayed inactive for every sample, verified 4,000 stored / parsed / unique
messages, had no drops / skips, normal recovery and an uninterrupted connection.
Manual scrolling exceeded 51,000 px with three reversals, and a matching message
was visible in a 594 px viewport. Observed input was 100.45/s and 100.25/s.
The basic-loop summary had no render thread, confirming the execution mode
actually changed. Its lower CPU came with substantially fewer frames, so it
does not prove equivalent-cadence efficiency or acceptable responsiveness.
The basic loop was not retained.

A separate `SessionSidebar.layer.enabled` probe compared two Release snapshots
that differed only in caching the connection sidebar. The source and main
artifact were restored without the layer before measurement. Both runs fixed
the background state and explicitly used the threaded loop:

| Sidebar | Thirty-second CPU | Whole-observation frames/s |
| --- | ---: | ---: |
| Uncached | 39.52% | 116.14 |
| Cached | 39.06% | 116.05 |

All integrity, continuous-connection, normal-recovery, visible-message and
background checks passed, with approximately 51,000 px movement / three
reversals and 100.25/s input. The 0.46-point difference in one pair does not
establish a reliable gain, especially given variation among reference runs.
Extra layer memory/compositing cost is not justified by this evidence. The
sidebar cache was not retained, and no visible QML change from these probes
remains. CPU traces and all experiments remain synthetic and isolated; none
prove the same-broker competitor target has been reached.

Debug and Release builds, QML lint and all 42 tests passed after adding the
fixed-background fixture (19.75 seconds for that suite). No measurements
overlapped builds/tests. Bash syntax and whitespace checks passed. The retained
background control provides stronger conditions for the next actual optimization
comparison, rather than treating a lower frame rate or different window state
as a success.

After restoring the experimental QML properties, the current Debug build,
QML lint and all 42 tests passed again (16.97 seconds). Whitespace checks passed;
the sidebar file has no residual experiment diff. No test application remained.
Installed competitor bundle metadata was read without opening an existing
connection: MQTTX 1.13.0 and MQTT Explorer 0.4.0-beta.6 are available locally.
Availability/version alone is not a comparison result. The next comparison must
verify isolated competitor settings, workload/display equivalence and account
for any helper processes, rather than comparing a single parent-process number
against this application's all-thread CPU.

### First Isolated MQTTX Comparison

MQTTX 1.13.0 was launched with a fresh temporary user-data directory, HOME,
APPDATA and configuration location. The APPDATA override also isolated its
legacy database migration path. Its shipped storage and migration code was
inspected read-only; installed application files were not modified. Runtime
`app.getPath('userData')` and `app.getPath('sessionData')` both matched the new
temporary directory. No existing competitor connection database was read.
The benchmark configuration disabled update checks and Copilot before the
measured launch. The first initialization-only launches were not measured.

The competitor connected to a loopback-only Mosquitto instance, subscribed to
`profile/#` with MQTT 5 / QoS 0 and a 30-second keepalive, and received the same
canonical 2 KB unique-sequence fixture used by the application wire benchmark.
The publisher and broker were separate from the measured application process
tree. The competitor window was visible, 1480 by 820 logical pixels, and
explicitly non-focusable; focus-event and final-state checks passed.

`scripts/measure-process-tree-cpu-macos.pl` measures an existing process and all
observed descendants, without launching or stopping them. It uses cumulative
CPU time, a monotonic clock and one-second process-tree checks. A changed tree,
identity, parent or backwards CPU counter invalidates the measurement. This
avoids silently accepting a parent-only Electron number. Children that both
start and exit between snapshots remain unobservable; the tool explicitly
reports that limitation rather than claiming perfect lifetime accounting.
Its self-test covers descendant selection, unrelated-process exclusion, CPU
time parsing, malformed input, missing roots and rejected tree/identity changes.
CTest registers that self-test when Perl is available.

| Scenario | MQTT Plus Release | MQTTX 1.13.0, four observed processes |
| --- | ---: | ---: |
| Connected idle, visible background window | 1.00% | 1.76% |
| Wire input, 100/s target, 2 KB, visible background window | 26.38% | 21.88% |

All numbers are a single thirty-second cumulative-CPU measurement after a
five-second warmup; 100% means one core. Neither builds nor profilers overlapped
these measurements. The live application verified 4,000 stored / successfully
parsed / unique messages, zero drops or skips, normal recovery, an uninterrupted
connection, 100.29/s observed input and the newest message visible in a 594 px
viewport. MQTTX's synthetic SQLite database independently verified all 4,000
unique sequences, exact 2 KB payload lengths and expected topic / QoS. Its own
page capture showed sequence 3999 visible at the followed tail. Approximately
94% of its measured process-tree CPU belonged to one helper process: measuring
only its parent would have missed most of the work.

The live pilot's difference is 4.50 percentage points, about 20.6% relative to
MQTTX. This is useful reproduction evidence, not an accepted performance target
or a completed fix. It is only one pair, uses synthetic rather than the user's
actual rate/payload, and does not prove equal visual work or frame cadence:
MQTTX displays a full plaintext bubble, while this application displays compact
message rows. The competitor's generator pacing log was not retained in this
first pilot; the next runner revision adds an explicit pacing gate. The idle
fixtures also differ: this application's existing connected-idle driver creates
one synthetic message, whereas MQTTX had zero. The idle numbers therefore do
not establish an equivalent populated-history comparison. Foreground, scrolling
and MQTT Explorer comparisons remain required before broad claims.

### Live Main-Thread Hotspot

A separate eight-second Time Profiler capture used the same isolated live-wire
scenario and explicit background-window control. This recording was diagnostic
only; its CPU measurement is not used for acceptance. Structured export analysis
filtered the target process and `Running` samples, rather than treating sleeping
render waits as CPU work.

Of 978 ms sampled CPU weight, the main thread accounted for 637 ms (65.13%),
the render thread 225 ms (23.01%), platform workers 64 ms (6.54%), the writer
40 ms (4.09%), parser 9 ms (0.92%) and admission 1 ms (0.10%). Within the main
thread, delegate reuse/model notification accounted for 481 ms inclusive,
text assignment 440 ms, text layout 432 ms and shaping 374 ms. These are nested
stacks and must not be added. CoreText advance loading accounted for 169 ms
inclusive, including 94 ms self-time in its memory initialization path.
The next investigation is therefore redundant text layout during delegate
reuse, not reducing message ingestion or persistence correctness. Qt's
[CoreText font-engine implementation](https://github.com/qt/qtbase/blob/6.11/src/gui/text/coretext/qfontengine_coretext.mm)
provides the corresponding advance-loading implementation; this evidence does
not by itself establish a Qt defect.

A payload-only delayed-binding prototype compiled and passed QML lint. Its
benchmark launch was not executed because the automatic approval service hit
an account usage limit. That is an approval-service failure, not an unsafe-action
verdict or an application failure. The prototype was removed from source before
further validation; only private generated comparison snapshots were retained.
No gain is claimed. A delayed binding can coalesce intermediate values but also
adds runtime cost, as documented by
[Qt Binding](https://doc.qt.io/qt-6/qml-qtqml-binding.html#delayed-prop), so it
requires actual repeated CPU and display-correctness evidence before retention.

Raw traces, test screenshots, temporary profiles and prototype scripts remain
outside Git. Public evidence omits private temporary absolute paths and process
IDs. All test instances and brokers from the completed runs exited; the rejected
prototype launch created no process. The overall performance goal remains open.

After removing the unmeasured prototype, Debug and Release builds and QML lint
passed. All 43 registered tests passed (29.85 seconds), including the new
process-tree measurement self-test. Perl syntax and whitespace checks passed.
These checks validate the retained code and measurement-tool invariants, not
the unresolved competitor CPU target.

### Retained Mac ASCII Payload Rendering Path

After the approval service recovered, the delayed-binding prototype measured
27.01% against an adjacent 27.11% reference. This does not establish a meaningful
gain, and the delayed binding remains removed.

A different prototype combined native text rendering with skipping optional
shaping for ASCII payloads. This is not the previously rejected standalone
shaping change: the renderer changes as well, addressing its earlier increase
in render-thread cost. Two adjacent live-wire pairs used AB / BA order:

| Pair | Default text CPU | Native ASCII text CPU |
| --- | ---: | ---: |
| AB | 27.11% | 24.81% |
| BA | 26.91% | 24.21% |
| Median | 27.01% | 24.51% |

The reduction is 2.50 percentage points, 9.26% relative, in this synthetic
live-input fixture. Whole-observation frame rates were 41.72 / 43.21 for the
references and 41.75 / 42.57 for the prototype. No frame-rate setting or input
rate was reduced. Every run verified 4,000 stored / parsed / unique 2 KB
messages, zero drops / skipped parses, normal recovery, an uninterrupted
connection, the latest message visible and strictly inactive windows.

Scrolling was checked separately with reversed order, BA / AB:

| Pair | Default text CPU / frames per second | Native ASCII CPU / frames per second |
| --- | ---: | ---: |
| BA | 36.16% / 116.25 | 36.67% / 116.44 |
| AB | 36.93% / 115.71 | 36.10% / 115.24 |

These close, mixed-direction CPU differences do not establish a scrolling gain.
They also do not show a material frame-rate regression in this fixture. All
four runs verified full capture and parsing, approximately 50,000 px of manual
wheel scrolling, three reversals and a visible message in a 594 px viewport.
The frozen visible windows contained 201-203 rows because the first wheel event
occurs around the 200-row admission threshold. This variation is reported rather
than treating the scrolling display workload as perfectly identical.

The retained source restricts the path to macOS, ASCII payloads and compact /
hover display. Non-ASCII text and other platforms keep the existing shaping and
renderer. Fully expanded payloads also keep the scalable Qt renderer, avoiding
introducing a large native bitmap for long inline content. The font family,
font size, payload data, three-/twelve-line preview limits and full-content
behavior are unchanged. Optional Latin ligatures may differ in the ASCII fast
path; the underlying text and copy/inspection data are not modified. See
[Qt text rendering](https://doc.qt.io/qt-6/qml-qtquick-text.html#renderType-prop).

The architecture test evaluates the actual source guard with Qt's JavaScript
engine across three platforms, three display modes and seven payload cases:
empty text, JSON / Latin text, whitespace, Chinese, combining marks, Arabic and
an emoji. It also checks the guard is connected to both rendering properties.
This covers selection of the default path for complex text, not a claim of
rendered-font QA on Windows/Linux.

The profiler now has an explicit `--profile-capture-window` option, exposed by
`MQTT_PROFILE_CAPTURE_WINDOW=1` in the runner. It grabs only its own isolated
QQuickWindow after the CPU/statistics snapshot and writes a new private temporary
PNG. It does not capture the desktop or other applications, and is off by
default. Capture success and image dimensions are gated; only the generated
basename is printed. Default/native synthetic window images were inspected:
message text, clipping, metadata, controls and the manual-follow state rendered
without new layout overlap. Images remain private and outside Git. Source tests
guard the opt-in, own-window API, temporary-file ownership and snapshot ordering.

All 43 tests passed after the platform/full-display guards and capture diagnostics
were added (15.41 seconds). The goal remains open: this is a measured partial
improvement, not proof that actual-user idle/live/scroll averages now match
MQTTX or MQTT Explorer, nor approval to promote a release.

Final guarded-source Release validation measured 21.68% in the same live-input
fixture, with 42.62 frames per second over the whole observation. This is a
single subsequent run, not an adjacent A/B comparison; its lower absolute CPU
must not be attributed to an additional improvement. The runtime diagnostic
confirmed `tailNativeAsciiPayload=true`, so the real macOS/platform/display-mode
guard selected the measured path. All 4,000 messages were stored, parsed and
sequence-verified with no drops, continuous connection, normal pressure and the
newest message visible. Its opt-in own-window image rendered correctly.
The provisional 20%-of-one-core local budget still failed; that budget is not
the user's competitor-relative acceptance criterion.

Connected-idle validation of the same build measured 1.13%, passed the local
3% budget and kept the connection through the 30-second keepalive interval.
This fixture contains only one synthetic row, so it does not establish idle
behavior after a large real session or equal competitor workloads.

A subsequent eight-second Time Profiler attachment to the final guarded build
collected 476 ms of target-process Running samples: main thread 276 ms (57.98%),
platform workers 96 ms (20.17%), render thread 73 ms (15.34%), history writer
19 ms, parser 6 ms and admission 5 ms. Main-thread inclusive samples still
centered on delegate reuse/model notification (174-178 ms), text setting
(102 ms) and text layout (85-89 ms); text shaping accounted for 33 ms.
These nested inclusive values must not be added together. They identify
remaining diagnostic targets, not independently measured CPU savings.

The traced run reported only 8.63% CPU despite unchanged input and complete
4,000-message integrity. Because the attachment changed observation conditions
substantially, that CPU value is explicitly excluded from acceptance and A/B
results, just like the earlier traced scrolling run. Raw traces remain private.
All instances and local brokers from these completed runs exited. Release build,
Debug QML lint, benchmark-shell syntax, process-tree self-test and whitespace
checks passed in addition to the 43-test suite recorded above.

### Rejected Hidden Divider Text Prototype

A prototype left the hidden divider label empty for ordinary message rows,
while retaining the original divider title when visible. Two live-input pairs
used AB / BA order:

| Pair | Retained source CPU / frames per second | Hidden-title prototype CPU / frames per second |
| --- | ---: | ---: |
| AB | 23.41% / 41.77 | 24.91% / 42.10 |
| BA | 23.85% / 41.81 | 24.84% / 42.04 |

Every run retained all 4,000 stored / parsed / unique messages, normal pressure,
an uninterrupted connection, a visible newest message and strict background
window state. The prototype provided no measured gain and was removed; the
retained renderer/platform guards remain unchanged. This single-topic fixture
has a stable title/alias, so this result does not rule out hidden-label costs
with other topic/title distributions. It does rule out retaining this change
as an established improvement for the current live-input reproduction.

### Populated Connected-Idle Fixture

The runner now accepts `broker-populated-idle`. Unlike the one-row
`connected-idle` fixture, it first receives 4,000 unique 2 KB JSON messages
over the local broker at 100/s. It waits for both publisher and generator to
exit and for the application's admission-complete marker, then waits five
seconds before measuring idle CPU. No message publisher remains during the
CPU window. The application stays connected and retains its saturated
1,200-row model window and the full 4,000-message persisted history. The
overall observation is kept long enough for the post-input measurement and
recovery, with integrity, rate, sequence, keepalive, background and tail gates.

```bash
MQTT_PROFILE_BACKGROUND_WINDOW=1 bash scripts/benchmark-cpu-macos.sh \
  "$APP" broker-populated-idle 3 30
```

Final retained-source Release validation measured 1.10% over the 30-second idle
window, with all correctness gates passing. The own-window image still showed
the newest sequence and 4,000 captured messages, zero current traffic and an
active connection. The driver-reported frame rate and per-thread percentages
span the entire approximately 80-second observation, including the preceding
40-second population phase; they must not be interpreted as idle-only metrics.
This closes a missing large-history idle measurement in the synthetic fixture,
not the user's real broker/topic-rate or foreground competitor acceptance.

MQTTX 1.13.0 was then tested with a fresh isolated profile, the same 4,000-message
population, publisher exit before warmup, a five-second idle warmup and a
30-second process-tree CPU window. It measured 1.20% across four stable observed
processes (0.36 CPU seconds in 30.033 wall seconds). Its database contained all
4,000 unique sequences with the expected topic, QoS and 2 KB payload; the broker
confirmed subscriber keepalive responses with no subscriber disconnection.
The window remained visible, non-focusable and never focused. Its private
own-page image showed the newest sequence and the 4,000-message counter.

An initial comparator attempt was discarded because the test incorrectly
looked for a text-named Disconnect button when the connected-state control is
icon-only. This was a validation-locator error, not evidence of an application
disconnection. The completed attempt used the broker's subscriber identity,
disconnect log and keepalive response instead, without changing the competitor.
Both attempts cleaned up their own instances and publishers.

The 1.10% / 1.20% results are only one populated-idle comparison with fresh demo
profiles. They do not establish a meaningful CPU advantage or a resolved
real-user deficit. The displays still differ (compact formatted Qt rows versus
MQTTX's default full plaintext bubble), and foreground, other topics, existing
settings and MQTT Explorer remain unverified. They do provide evidence that
4,000-message history alone does not reproduce a 20% idle excess here, narrowing
the next investigation without claiming broad completion.

### Rejected Manual Row Layer Pilot

A separate pilot enabled an offscreen layer only for active, manually viewed
rows using the existing Mac ASCII compact/hover path. Automatic follow and
fully expanded/non-ASCII rendering were not changed. An adjacent scrolling
pair measured 34.83% / 116.17 frames per second without the layer and
36.06% / 116.52 with it. Both runs stored / parsed / verified all 4,000 messages
at 100/s, maintained an inactive visible window and an uninterrupted connection,
and produced real manual scrolling with three reversals and correct own-window
images. Frozen windows differed slightly (200 / 202 rows), as did scroll
distance (51,061 / 52,144 px) and event counts (2,351 / 2,419).

This single pair establishes no CPU benefit; it is not sufficient to quantify
a general regression, given the small workload variation. The layer was removed
rather than retaining extra texture/compositing costs without evidence. Qt's
[layer memory/performance guidance](https://doc.qt.io/qt-6/qml-qtquick-item.html#memory-and-performance)
also cautions against assuming that offscreen caching is a general-purpose
rendering optimization. No row layer, render-backend switch or frame-rate cap
was retained from this pilot.

After removing both prototypes, the final Debug and Release builds and QML
lint passed. All 43 tests passed again (15.22 seconds); benchmark-shell syntax
and whitespace checks passed. All completed test instances, publishers and
brokers were cleaned up. The performance goal remains open, particularly for
foreground/real-user workloads and comparative scrolling/MQTT Explorer evidence.

### Foreground Validation Is Still Unavailable

A populated-idle foreground attempt retained all 4,000 messages and the full
1,200-row model window, but had no active-window samples after warmup and ended
inactive. Its 0.93% CPU value is explicitly excluded from foreground acceptance.
The normal `QWindow::requestActivate()` request did not establish a continuously
active window in this environment; this result does not establish a product or
operating-system defect.

The private MQTTX fixture now has a separate foreground mode using the official
[Electron application-focus API](https://www.electronjs.org/docs/latest/api/app#appfocusoptions)
once at setup, not continuously during measurement. It verifies focused state
before starting CPU accounting and records any subsequent blur. Its foreground
live-input attempt failed that initial state gate, so CPU accounting never
started and no foreground competitor result is claimed.

Connection-fixture checks were strengthened as well. The completed prior
populated-idle MQTTX database and the new setup confirmed MQTT 5.0, a 30-second
keepalive, clean session, plaintext loopback transport and QoS 0. Those match
the Qt fixture rather than assuming the competitor's protocol defaults.
Only synthetic temporary databases were read.

The available desktop provider reported no direct window-focus capability.
A normal title-bar click was attempted on a PID-verified private one-row Qt
diagnostic instance, using only that window's geometry and `--no-screenshot`.
The provider refused the action because Accessibility permission was missing.
No click or desktop screenshot occurred. No permission settings were changed
and no alternative input method was used to bypass the denial. The diagnostic
instance was explicitly terminated after rechecking its PID, parent and private
executable identity; the completed benchmark attempts cleaned up their own
applications, publishers and brokers.

The runner now verifies a five-second foreground-ready marker before CPU
measurement, and before waiting for population in the populated-idle case.
The marker is a precise timer confined to requested profiler activation.
Unsupported binaries are rejected before launch. A false marker stops the run
instead of spending a CPU measurement window on an inactive application. The
existing final continuous-foreground gates remain in place, since being active
once does not establish that the window stayed active. This improves measurement
validity and turnaround, not production CPU, and is not a completed foreground
or real-user performance result.

Runtime validation of the new gate printed `foregroundReady=false`, exited with
status 2, cleaned up the test instance/publisher/broker and explicitly reported
that CPU measurement had not started. The obsolete inactive-window CPU window
was no longer run. A separate short background regression captured, parsed,
stored and sequence-verified all 1,100 messages, preserved the latest visible
tail and passed the unchanged background/connection/recovery gates. Its one-
second CPU sample and deliberately permissive budget are functional-test output
only, excluded from performance acceptance.

Debug and Release builds passed. All 43 tests passed (15.22 seconds), including
the new architecture guard for the precise, opt-in timer, early runner check and
retained final foreground-loss gate. This source-contract test complements the
negative runtime check; it does not prove successful foreground measurement.
The actual-system/rate/payload/build comparison conditions were requested again
because the observed real-user idle excess still lacks an aligned reproduction.
The performance goal remains open.

A read-only lookup found no running process under this project's
`mqtt_plus_app` executable name after cleanup. In this continuation, no existing
real-user session was attached, restarted, inspected for credentials or modified.

## Synchronize The Footer With Visible Message Batches

The next bounded hypothesis was that the independent 50 ms display-count timer
could request an extra scene update between the existing 33 ms message-list
batches. The retained change synchronizes the display count on the filtered
model's actual `rowsInserted` notification and cancels the pending display-count
timer. It does not change message admission, parsing, storage, list batching,
follow-tail timing, wheel input or the configured frame-rate limit.

Only non-paused visible model insertions take this path. Filtered-out messages,
paused output and frozen history without model insertions retain the existing
50/250 ms fallback timers. The exact total-count notifications remain unchanged.
This is not wired to `messageRowsAppended`, which can fire without a model
insertion when reading frozen history.

The regression test was run before the change: the visible-row case failed
because the footer still waited for its independent timer; paused and
filtered-out cases passed. After the change all three cases passed, including
the eventual fallback count and absence of duplicate display notifications.
The full Workbench test executable passed 38 cases. The test demonstrates prompt
count synchronization with model insertion, not screen presentation latency.

Release binaries from immediately before and after this single change were
compared in A/B then B/A order. Each run used the same isolated localhost MQTT
5 broker, QoS 0 subscription, 4,000 unique 2 KB JSON messages at approximately
100 messages/second, 1,200-row live model and a 30-second CPU window. These are
background-window measurements, not successful foreground or actual-user
comparisons. CPU percentages below use 100% for one core.

| Order | Before CPU | After CPU | Before rendered frames/s | After rendered frames/s |
| --- | ---: | ---: | ---: | ---: |
| A then B | 24.84% | 23.15% | 43.93 | 28.09 |
| B then A | 25.88% | 22.81% | 42.36 | 27.97 |

The two-run medians are 25.36% before and 22.98% after: 2.38 percentage points,
or approximately 9.4% relative reduction. Both orderings improved. All four
runs received, parsed, stored and sequence-verified all 4,000 messages with no
drops or pressure skipping, stayed connected, and ended with the latest tail
fully visible. Input rates were 100.29-100.39 messages/second. The deliberately
permissive 999% runner budget was used to collect observations, not as a product
acceptance threshold.

Raw rendered-frame counts decreased, consistent with eliminating counter-only
updates. This alone is not evidence of faster or equally fresh message display.
The change leaves the list's batching/follow-tail code untouched and the unit
test checks that the counter updates immediately with insertion rather than
waiting longer. A successful foreground presentation-latency comparison remains
outstanding; these results do not establish the real-user performance goal.

A separate B/A manual-scroll regression produced 38.39% after versus 37.93%
before, with 115.86 versus 116.37 rendered frames/second. Both runs verified all
4,000 messages, three direction reversals, visible historical messages and the
same 204-row frozen window. Wheel events were 2,414 versus 2,407 and distances
were approximately 52.51 versus 52.41 thousand pixels. This single close pair
shows no scroll optimization and is not sufficient to claim either a CPU gain
or absence of small regressions. No global frame cap was lowered.

The counter synchronization is retained for its repeatable live-input benefit
and focused behavior tests. Idle and scroll excess versus competitors, real
input/build conditions and foreground validation remain unresolved. Results
must not be added arithmetically to earlier gains from different baselines.
All benchmark instances, publishers and brokers were cleaned up. Raw logs and
private binary snapshots remain outside Git; personal paths are redacted here.
Debug and Release builds passed, all 43 registered tests passed (18.99 seconds),
and `git diff --check` passed after the change. No QML was changed in this probe.

## Remaining Scroll Diagnosis And Reproduction Gap

An eight-second Time Profiler capture of the current Release scroll fixture
contained 1,385 ms of weighted `Running` samples. The render thread accounted
for 518 ms (37.4%), main thread 446 ms (32.2%), unnamed platform workers 280 ms
(20.2%), display-link thread 46 ms (3.3%), writer 35 ms (2.5%), admission 28 ms
(2.0%) and parser 22 ms (1.6%). The XML analyzer now explicitly preserves
samples with unavailable backtraces rather than failing or dropping their CPU
weight; two writer milliseconds had no backtrace.

Render-thread inclusive stacks included render-pass setup (161 ms), preparation
(118 ms) and recording (109 ms). Platform worker samples included Metal command
submission (123 ms). Main-thread inclusive stacks included ListView item
geometry changes (72 ms), viewport movement (60 ms), refill/layout (50 ms),
text replacement (40 ms) and text layout (35 ms). These nested totals must not
be added. This supports investigating view/render work before storage or parsing;
it does not establish a Qt or Metal defect or justify reducing frame rate.

The overlapping runner reported 13.56% CPU, approximately 115.69 rendered
frames/second, 1,910 wheel events and 45.17 thousand pixels of travel. All 4,000
messages passed integrity checks. The CPU observation is excluded from
acceptance: tracing changes execution and this workload differs from untraced
scroll runs. No production change was made from this capture.

An isolated MQTTX 1.13.0 pilot then established an actual wheel-driven historical
message view. It used a fresh synthetic profile, localhost MQTT 5/QoS 0,
30-second keepalive, 4,000 unique 2 KB messages at 100.02/second, a 1480 by 820
background window and the existing all-process CPU accounting. The observed
message-list element was discovered from visible DOM geometry rather than
guessed; its CSS selector was used only after that inspection. Playwright sent
20-pixel wheel deltas with a target 16 ms interval. The Node controller was
outside MQTTX's measured process tree. No installed competitor source or normal
profile was edited.

The completed run stored and sequence-verified all 4,000 messages, retained its
connection/keepalive and remained visibly in historical messages. It processed
2,256 wheel inputs over 36.10 seconds, travelled approximately 45.10 thousand
pixels and reversed direction twice. The post-measurement owned-window image
showed a historical sequence, total 4,000 and an unread-message indicator.
Four stable observed MQTTX processes used 49.66% CPU over 30.02 seconds.

This is **not an equal-work scroll comparison or acceptance result**. MQTTX's
default view wraps full plaintext payloads; this project's default view presents
compact formatted previews. Viewport heights, retained display windows, scroll
start times, reversal counts and wheel units differ (browser pixels versus Qt
angle deltas). Competitor rendered-frame and presentation-latency parity was
not established. The pilot does not prove this project is faster, nor does it
reproduce the reported 20% excess. It establishes that the existing synthetic
scroll fixture cannot by itself settle the real-user report.

One preliminary competitor inspection exited during page loading because the
document body was not ready. Its CPU measurement never started. The wait was
corrected to tolerate an absent body, and the subsequent inspection and scroll
run completed. Every attempt cleaned up its owned app, generator, publisher and
broker. Logs, profiles, images and traces remain private and outside Git;
personal paths and process identities are redacted from this document.

The remaining acceptance gap is now a real affected build/runtime scenario,
including foreground behavior, actual message characteristics and user settings.
No further rendering change is justified solely by the unmatched pilot.

## First Real-Business Observation

The user subsequently launched both normal-business applications. Read-only
process inspection identified this project's Release executable without the
synthetic profiler argument and the installed MQTTX 1.13.1 executable. Neither
application was launched, restarted, terminated, focused, disconnected or
reconfigured by this observation. Process identities and personal paths are
redacted here. No desktop screenshot was taken.

A first unpaired 30-second project CPU observation was 18.78%; this is not a
competitor comparison. Two later simultaneous 30-second observations used the
existing stable-process-tree accounting for both apps (one observed project
process and four observed MQTTX processes):

| UTC Window | Project CPU | MQTTX Total CPU | Project New Stored Incoming Rows | MQTTX New Stored Incoming Rows |
| --- | ---: | ---: | ---: | ---: |
| 08:38:08-08:38:38 | 13.25% | 18.08% | Not measured | Not measured |
| 08:44:03-08:44:33 | 11.32% | 53.05% | 3,409 | 7,539 |

These windows took place on 2026-10-08; 100% represents one CPU core. Their
process trees were stable, but user interaction state, foreground parity and
the first window's input counts were not established. The second window's
stored-row counts differ substantially. These observations therefore **do not
prove equal-workload performance or completion**, even though the project's
CPU was lower. A relative speedup must not be calculated from them.

The second observation used read-only SQLite aggregate queries before and
after CPU measurement, not during it. Watermarks were the project's maximum
message ID and MQTTX's maximum message rowid. The ending queries counted only
new incoming records, excluding outgoing records. Persistence batching at
window boundaries can affect counts; these are not loss/completeness assertions.
No message contents, topic names, connection identifiers or broker credentials
were returned. No settings were changed and no normal databases were copied.

The new project rows averaged 109.35 raw payload bytes (15-2,027), spanning
three topics. MQTTX rows averaged 274.71 stored-text bytes (90-5,067), spanning
two topics. The byte statistics use different representations and must not be
treated as identical raw payload measurements. A separate project aggregate
found 6,815 incoming records in a preceding 60-second interval, and its last
1,000 incoming records averaged 111.14 raw bytes. This demonstrates that the
earlier uniform 2 KB fixture is not representative of the observed size mix.

Anonymous grouped aggregates found one recent incoming session in this project
and one connection in MQTTX's last 8,000 incoming records. That does not rule
out overlapping subscriptions, capture policies, historical/redelivered data
or other differences. One MQTTX aggregate initially encountered a database
lock; a bounded one-second read-only retry completed. It was outside the CPU
measurement window. The cause of the differing stored-row counts is unproven;
the user was asked to verify subscription/capture conditions before acceptance.

A separate eight-second CPU trace of the running normal project process
contained 1,500 ms of weighted Running samples: main 688 ms (45.9%), render
446 ms (29.7%), platform workers 298 ms (19.9%), writer 16 ms (1.1%) and
admission 11 ms (0.7%). Main-thread inclusive stacks included delegate reuse
(117 ms), text sizing (96 ms), text replacement (78 ms), text layout (74 ms)
and layout rearrangement (33 ms). Render-thread inclusive stacks included
render preparation (173 ms), render recording (138 ms) and scene synchronization
(70 ms). These nested durations must not be added or used as untraced CPU
acceptance. They continue to prioritize view/layout/render work, but do not
justify a new production change without a representative controlled loop.

All measurement/trace handles completed. The normal business apps were left
running. Private traces and aggregate reports remain outside Git, with no raw
business content added to this document. The original idle/live/scroll goal
remains active pending matched-input and interaction-state validation.

### Anonymous Topic-Level Follow-Up

Both normal apps were still running for a further simultaneous 30-second
observation, at 08:51:52-08:52:22 UTC on 2026-10-08. The project used 11.06%
CPU; MQTTX's four stable observed processes used 51.19%. Again this is not an
equal-work performance result. New stored incoming rows were 3,400 versus
6,794. Anonymous topic comparison localized the discrepancy:

| Shared Anonymous Topic | Project Rows | MQTTX Rows |
| --- | ---: | ---: |
| topic-1 | 1,539 | 3,080 |
| topic-2 | 10 | 0 |
| topic-3 | 1,851 | 3,714 |

The two common topics each have approximately twice as many MQTTX stored rows;
the difference is not explained solely by the project's additional topic.
Topic names were compared transiently in the private helper and replaced by
shared ordinal labels before output or report storage. No raw names or message
contents were retained in the report. Counts are still persistence observations,
not proven network input counts or completeness assertions. The user has not
yet confirmed matched subscriptions/capture settings or interaction state.

A preliminary attempt encountered a read-only database lock at the initial
MQTTX watermark query and exited before CPU measurement. A subsequent bounded
query succeeded, followed by the completed observation above. Queries have a
one-second maximum lock wait; no application/database restart, copy or write
was used to work around locks.

An aggregate check of the last 1,000 MQTTX incoming records found 1,000 distinct
groups when grouped by topic, stored payload, QoS, retain and creation value:
no exactly duplicated full records in that sample. A separate same-topic
preceding-payload check found no equal stored payloads among 998 comparisons.
Its SQLite datetime conversions produced no valid time gaps, so the attempted
time-gap diagnostic is unusable. These checks do not establish the cause of
the doubled counts, and do not establish matching semantic/raw payloads across
apps. In particular, duplication at the network layer must not be asserted
from the topic counts alone.

A later read-only TCP metadata check found one established project connection
and no established TCP entries for the observed MQTTX process tree at that
instant. Both main processes and the same MQTTX child processes were still
present. This does not establish MQTTX's transport/state during the earlier
measurement or why no entry was returned, and must not be used as proof of a
broker mismatch or earlier disconnection. No network packets were captured.

The apps were left running and no subscriptions or capture policies were
changed. The remaining fair-comparison/idle/scroll acceptance gates remain
open. This continuation produced diagnostic evidence only, not a production
optimization or a completed performance claim.
