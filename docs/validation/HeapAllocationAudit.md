# Firmware Heap Allocation Audit

> **Doc class:** Evaluation evidence. Snapshot date: 2026-08-21. This is a source/build audit, not a long-duration
> hardware heap-fragmentation receipt.

## Scope and method

This audit covers the production firmware under `firmware/src/` and `firmware/include/`. It searches for Arduino
`String`, dynamic ArduinoJson documents, STL containers/callbacks, and direct allocation APIs, then classifies each use
by lifecycle:

- **steady-state:** recurring while the instrument performs
- **periodic when connected:** recurring only while host streaming is enabled
- **burst:** bounded command, transfer, save/load, or profile operation
- **boot/setup:** constructed before the steady-state loop
- **test-only:** absent from the production image

Risk ranks combine frequency, size variability, lifetime overlap, and whether capacity is reused. They do not assume
that every `String` or STL type is defective.

## Production build baseline

Command:

```bash
pio run -d firmware -e teensy40_main
```

Observed successful build:

| Region | Linked usage | Reported headroom |
| --- | ---: | ---: |
| Flash | 385,548 bytes code + 43,200 bytes data + 8,496 bytes headers | 1,594,372 bytes for files |
| RAM1 | 66,080 bytes variables + 374,552 bytes code + 18,664 bytes padding | 64,992 bytes for locals |
| RAM2 | 283,296 bytes variables | 240,992 bytes for `malloc`/`new` |

The RAM2 figure is link-time headroom before runtime allocations. It does not reveal the largest free block, allocation
churn, or fragmentation after hours of streaming and repeated configuration transfers.

## Allocation inventory

| Area | Lifecycle and frequency | Allocation behavior | Bound/reuse | Risk |
| --- | --- | --- | --- | --- |
| WebSerial scope telemetry | Connected periodic, every 50 ms | Builds a heap-backed trace ID and local serialized payload `String` for every frame | JSON document is fixed at 1,280 bytes; output string is recreated | **High sustained** |
| WebSerial state telemetry | Connected periodic, every 500 ms | One trace ID plus nine serialized payload strings per snapshot | JSON documents are fixed at 768–3,072 bytes; output strings are recreated | **High sustained** |
| Chunked config/mod-matrix reads | Burst; service runs every 1 ms and emits up to two frames | Persistent full payload plus a substring and serialized line `String` for each 20-byte frame | Source documents fixed in RAM2; payload size bounded by document capacity | **High burst** |
| Bulk `SET_ALL` assembly | Burst during Apply | Local chunk/error strings plus a persistent payload string; `reserve(current + chunk)` can grow repeatedly | 32,768-byte maximum; parse document fixed in RAM2 | **High burst/peak** |
| Chunked `SET_PROFILE` assembly | Burst during profile edit | Per-frame substring plus persistent payload; reserves an estimated total on frame zero | 12,288-byte maximum; parse document is fixed | Medium burst |
| `GET_SCHEMA` | Usually once per host handshake | Constructs a large temporary schema string | Explicit 9,400-byte reserve; released after response | Medium peak, low frequency |
| `GET_CONFIG` and modulation exports | Host command/handshake | Serializes large fixed documents into temporary strings; chunked mode then retains a copy | Documents fixed in RAM2; string peak depends on serialized output | Medium/high burst |
| Normal command dispatch | Serial task cadence, only when commands exist | Reuses one static command string reserved to `SERIAL_BUFFER_SIZE - 1`; some handlers create substrings | Primary command capacity reused; command lines are bounded | Low steady, medium under command flood |
| Display status/mode strings | Operator events | Two long-lived member strings change on status/mode updates | Small human-readable strings; not per display refresh | Low |
| Global vectors/maps | Boot plus explicit profile/config changes | Pot/follower/LED storage allocates at boot; EF maps and LFO routes can allocate on reload/edit | Long-lived; no per-DSP-tick growth found | Low steady, medium reconfiguration |
| Scheduler callbacks and task vectors | Setup | Tasks and callbacks are installed during scheduler initialization | Task vectors reserve 96 entries; no recurring insertion found | Low |
| SD hardware-config JSON | Boot only | `DynamicJsonDocument` sized from the optional file | Destroyed after boot parse | Low runtime |
| Startup LED colors | Boot animation | Vector assigned once for the LED count | Reused through startup, then remains long-lived | Low |

No production `std::queue<String>` and no application-owned `malloc`, `realloc`, `free`, or raw `new` call were found.
Static ArduinoJson documents dominate fixed scratch storage and are deliberately placed in RAM2 where large.

## Highest-risk path: connected telemetry

When WebSerial streaming is active, the scheduler calls scope telemetry at 20 Hz and state telemetry at 2 Hz. A state
snapshot emits nine JSON frames. That produces at least 38 serialized payload-string lifecycles per second:

- 20 scope frames per second
- 18 state frames per second

It also constructs approximately 22 trace-ID strings per second. This is the only identified path that repeatedly
allocates variable-length strings during otherwise normal connected performance without operator commands.

The JSON documents themselves are fixed-size stack objects. The avoidable churn is the `FrameMeta::traceId` string and
the local `String payload` inside `emitJson()`.

## High-burst paths

### Chunked reads

`GET_CONFIG_CHUNKED` and `GET_MOD_MATRIX_CHUNKED` first materialize the full serialized payload, then the 1 ms service
loop emits up to two 20-byte chunks. Each frame currently creates a payload substring and a serialized line string.
This can create hundreds of short-lived allocations during a single handshake even though it is not steady-state work.

The 20-byte chunk limit follows the native serial line guard, so increasing it is a protocol/transport decision rather
than a heap-only optimization.

### Bulk Apply

The `SET_ALL` assembler accepts at most 32,768 bytes. It keeps the ArduinoJson parse document in RAM2, but its assembled
text remains a heap-backed `String`. Calling `reserve(buffer.length() + chunk.length())` before each append can still
move the buffer as it grows. A future change should reserve a known transfer size once, use geometric capacity growth,
or move assembly into fixed RAM2 storage. That change needs malformed, timeout, overflow, and full-size transaction tests.

### Schema and full exports

Schema generation reserves about 9.4 KiB and is normally handshake-only. Full configuration and modulation responses
use fixed RAM2 JSON documents but temporary serialized strings. Their low frequency makes them lower priority than
telemetry, although their peak overlap with chunked-read state should be measured.

## Low-risk or intentionally dynamic areas

- Scheduler vectors reserve capacity during construction and are not extended during the normal loop.
- `std::function` callbacks are installed during setup or explicit manager configuration, not created per callback run.
- LED buffers, follower vectors, and core route tables are long-lived. Their allocation cost is primarily boot/setup.
- Display strings change on user-visible events rather than every render pass.
- The persistent command parser string reserves the maximum serial-line size once and reuses it.
- Large ArduinoJson documents for config, modulation, profiles, and Apply are static, with the largest placed in RAM2.

## Evidence gaps

Source inspection cannot answer:

- minimum free heap after boot, handshake, one hour of streaming, and repeated Apply/export cycles
- largest contiguous free block over time
- allocation failure count or high-water mark
- actual serialized telemetry/config/schema sizes on representative maximum configurations
- whether the allocator returns repeated telemetry strings to stable blocks or progressively fragments RAM2

A hardware receipt should sample total free heap and largest free block before/after a repeatable workload. Link-time
RAM headroom alone is not an endurance result.

## Recommended implementation order

1. **Connected telemetry:** replace per-frame trace and output strings with fixed or reusable storage, preserving exact
   JSON framing and unit-test log capture.
2. **Heap observability:** expose allocator totals/largest-block data through diagnostics when supported by the Teensy
   allocator, with a safe fallback when unavailable.
3. **Chunked reads:** remove per-frame substring/line allocation while retaining the existing wire contract.
4. **Bulk Apply:** change growth strategy only with full-size, overflow, abort, timeout, duplicate-ACK, and rollback
   coverage.
5. **Schema/export caching:** consider only after measurements show peak pressure or repeated host requests matter.

The first bounded code change should target WebSerial telemetry. It has the highest frequency, a narrow implementation
surface, fixed maximum document sizes, and existing simulator/unit/integration coverage for the emitted contract.
