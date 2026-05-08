# MN42 Line Protocol

This is the host-facing serial contract for MOARkNOBS-42 firmware. The transport is newline-delimited UTF-8 text over USB serial at `115200` baud. Commands are one line each. Responses are one JSON object per line unless the command is explicitly documented as a legacy text diagnostic.

## Transport Rules

| Rule            | Contract                                                                                                                                                                             |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Line ending     | Host sends `\n`; firmware trims whitespace before dispatch.                                                                                                                          |
| Command buffer  | Non-bulk command lines must fit `SERIAL_BUFFER_SIZE = 128` bytes including terminator.                                                                                               |
| Parser cadence  | Serial input polling and command queue dispatch run every `10 ms` by default from the mid-priority scheduler.                                                                        |
| Hot-path policy | Timer ISR only drops a MIDI service token. JSON parsing, command dispatch, EEPROM writes, profile loads, and telemetry emission stay out of interrupt context.                       |
| Streaming gate  | Telemetry and config patch events are silent until `HELLO` sets `webSerialStreaming = true`.                                                                                         |
| Response format | Current commands return JSON. Deprecated commands may return `{"type":"response","message":"... deprecated"}` or no payload when logging is disabled.                                |
| Error format    | Most validation errors return `{"type":"response","status":"error"}`. Bulk and JSON commands return `{"type":"error","code":"..."}`. Unknown commands log `Unknown command: <line>`. |

## Host Timeouts

These are host-side support boundaries used by the App runtime.

| Flow                                                                        |               Timeout |
| --------------------------------------------------------------------------- | --------------------: |
| Standard RPC, including `HELLO`, `GET_MANIFEST`, `GET_CONFIG`, `GET_SCHEMA` |             `3000 ms` |
| Full apply via chunked `SET_ALL`                                            |            `30000 ms` |
| Profile macro commands                                                      |             `6000 ms` |
| Scene commands                                                              |             `6000 ms` |
| Native `SET_ALL` line pacing                                                | `4 ms` between chunks |

## Chunking Rules

`SET_ALL` is the only intentionally chunked command. The App sends the final JSON frame in repeated lines:

```text
SET_ALL <chunk>
SET_ALL <chunk>
...
```

Firmware strips the `SET_ALL ` prefix and appends each chunk to `Utility::BulkConfigAssembler`.

| Limit                     |                                                              Value |
| ------------------------- | -----------------------------------------------------------------: |
| App chunk payload         |                                        `96` bytes after `SET_ALL ` |
| Firmware bulk payload cap |                                                      `32768` bytes |
| Firmware completion rule  | One balanced top-level JSON object, respecting strings and escapes |
| Required bulk identity    |                   `config_id` or `checksum` in the top-level frame |
| Idempotency key           |  `seq` plus `config_id`/`checksum`; duplicate last ACK is replayed |

Bulk frame shape:

```json
{
  "seq": 7,
  "config_id": "sha256-or-build-id",
  "config": {
    "slots": []
  }
}
```

Successful ACK:

```json
{ "type": "ack", "checksum": "sha256-or-build-id", "seq": 7 }
```

Bulk errors include `overflow`, `orphan`, `ingest`, `parse`, `checksum`, `config_missing`, `slots_missing`, `slots_size`, `slot_null`, `slot_type`, and `sysex_template`.

## Commands

| Command             | Request                                        | Response                                                                               | Notes                                                                           |
| ------------------- | ---------------------------------------------- | -------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------- |
| `HELLO`             | `HELLO`                                        | `{"hello":"mn42"}`                                                                     | Enables telemetry/config patch streaming.                                       |
| `GET_MANIFEST`      | `GET_MANIFEST`                                 | Manifest JSON                                                                          | See [Manifest Contract](ManifestContract.md).                                   |
| `GET_SCHEMA`        | `GET_SCHEMA`                                   | JSON schema                                                                            | Returns `ConfigManager::makeSchema()`.                                          |
| `GET_CONFIG`        | `GET_CONFIG`                                   | Full config JSON                                                                       | Includes pots, slots, EF routing, ARG, filter, LED state.                       |
| `GET_PROFILE`       | `GET_PROFILE` or `GET_PROFILE,<id>`            | Profile JSON                                                                           | `id` is `0..3`; omitted uses active profile.                                    |
| `SET_PROFILE`       | `SET_PROFILE,<id>,<json>`                      | `{"type":"response","status":"ok"}`                                                    | Sparse profile patch merged onto captured profile.                              |
| `SAVE_PROFILE`      | `SAVE_PROFILE,<id>`                            | `{"profile":<id>,"profile_saved":true}`                                                | Saves live deck to profile `0..3`.                                              |
| `LOAD_PROFILE`      | `LOAD_PROFILE,<id>`                            | `{"profile":<id>,"profile_loaded":true}`                                               | Loads profile `0..3`. Empty profiles get baseline defaults.                     |
| `RESET_PROFILE`     | `RESET_PROFILE,<id>`                           | `{"profile":<id>,"profile_reset":true}`                                                | Resets profile `0..3` to baseline.                                              |
| `SAVE_MACRO_SLOT`   | `SAVE_MACRO_SLOT`                              | `{"macro_saved":true,"macro_available":true}`                                          | Stores current macro snapshot.                                                  |
| `RECALL_MACRO_SLOT` | `RECALL_MACRO_SLOT`                            | `{"macro_recalled":true,"macro_available":true}`                                       | Restores stored macro snapshot.                                                 |
| `SET_ALL`           | Chunked bulk JSON                              | ACK or bulk error                                                                      | See chunking rules above.                                                       |
| `SET_SLOT_VALUE`    | `SET_SLOT_VALUE,<slot>,<value>`                | `{"type":"response","status":"ok"}`                                                    | Injects MIDI value `0..127` for slot `0..41`.                                   |
| `SET_POT`           | `SET_POT,<pot>,<channel>,<cc>`                 | OK or error JSON                                                                       | `pot 0..41`, `channel 1..16`, `cc 0..127`.                                      |
| `SET_LED`           | `SET_LED,<brightness>,<r>,<g>,<b>`             | OK or error JSON                                                                       | Firmware clamps brightness to board profile cap. RGB fields are `0..255`.       |
| `SET_EF`            | `SET_EF,<slot>,<ef>`                           | OK or error JSON                                                                       | Assigns slot to envelope follower.                                              |
| `SET_EF_IDLE_FLOOR` | `SET_EF_IDLE_FLOOR,<value>`                    | `{"type":"response","status":"ok","command":"SET_EF_IDLE_FLOOR","idle_floor":<value>}` | Clamped `0..127`.                                                               |
| `SET_ARGMETHOD`     | `SET_ARGMETHOD<method>`                        | OK or error JSON                                                                       | Legacy parser reads from character offset 14; prefer bulk config for new tools. |
| `ENTER_CONFIG_MODE` | `ENTER_CONFIG_MODE`                            | `{"type":"response","status":"ok","command":"ENTER_CONFIG_MODE","rebooting":true}`     | Requests one-shot USB configurator boot then reboots outside unit tests.        |
| JSON `GET_SCENES`   | `{"cmd":"GET_SCENES"}`                         | `{"cmd":"GET_SCENES","scenes":[...]}`                                                  | JSON scene commands short-circuit before legacy dispatch.                       |
| JSON `SAVE_SCENE`   | `{"cmd":"SAVE_SCENE","slot":0,"name":"Verse"}` | Scene result JSON                                                                      | Scene slot range is firmware-defined by `SceneStorage::kSceneSlotCount`.        |
| JSON `RECALL_SCENE` | `{"cmd":"RECALL_SCENE","slot":0}`              | Scene result JSON                                                                      | Applies stored scene if available.                                              |

## Deprecated Commands

These remain accepted for older tools but should not be used for new App or Bridge work.

| Command           | Current behavior                                                                   |
| ----------------- | ---------------------------------------------------------------------------------- |
| `GET_ALL`         | Logs `GET_ALL deprecated, use GET_CONFIG` when serial logging is enabled.          |
| `GET_ARGMETHOD`   | Logs `get_arg_method deprecated`.                                                  |
| `GET_BROWNOUTS`   | Logs `get_brownouts deprecated`.                                                   |
| `GET_EF`          | Logs `get_ef deprecated` or error for invalid slot.                                |
| `GET_LED`         | Logs `get_led deprecated` when serial logging is enabled.                          |
| `CAL_ENVS`        | Calibrates all envelope followers and returns OK.                                  |
| `GET_FILTER`      | Logs `GET_FILTER deprecated`.                                                      |
| `SET_FILTER`      | Seeds slot envelope payloads and returns OK. Prefer `SET_ALL`.                     |
| `GET_SLOT_FILTER` | Logs `GET_SLOT_FILTER deprecated` or error for invalid slot.                       |
| `SET_SLOT_FILTER` | Updates one slot filter payload and returns OK. Prefer `SET_ALL`.                  |
| `GET_ARGPAIR`     | Logs `GET_ARGPAIR deprecated`.                                                     |
| `SET_ARGPAIR`     | Updates global ARG enable/source pair and every slot ARG source. Prefer `SET_ALL`. |

## Telemetry

Telemetry starts after `HELLO` and runs from the low-priority scheduler every `100 ms` while streaming is enabled.

Each snapshot uses one shared `timestamp`, `timestampMs`, and `traceId`, then emits six JSON lines:

| Scope               | Contents                                       | JSON document capacity |
| ------------------- | ---------------------------------------------- | ---------------------: |
| `state_slots`       | 42 slot values plus `currentSlot`              |           `1024` bytes |
| `state_args_0_13`   | ARG config for slots 0-13                      |           `2048` bytes |
| `state_args_14_27`  | ARG config for slots 14-27                     |           `2048` bytes |
| `state_args_28_41`  | ARG config for slots 28-41                     |           `2048` bytes |
| `state_diagnostics` | Global ARG settings and diagnostic counters    |            `512` bytes |
| `state_envelopes`   | Six EF levels, two LFO values, EF active flags |           `1024` bytes |

Worst-case scheduled flow is 10 snapshots/s, 6 lines/snapshot, and up to `8704` serialized JSON bytes per snapshot by document capacity. That is `60` telemetry lines/s and `87040` bytes/s of worst-case JSON budget before USB framing. Typical payloads are smaller.

Patch events are event-driven and separate from the 100 ms telemetry stream:

| Event                                            | Trigger                    | Notes                                       |
| ------------------------------------------------ | -------------------------- | ------------------------------------------- |
| `slot_patch` plus legacy `config-patch`          | Slot edit from hardware UI | Modern payload max document is `896` bytes. |
| `envelope_assignment` plus legacy `config-patch` | EF assignment changes      | Compact assignment payload.                 |
| `filter_patch` plus legacy `config-patch`        | Filter tuning changes      | Compact filter payload.                     |
| `arg_patch` plus legacy `config-patch`           | ARG method/source changes  | Compact ARG payload.                        |

## Examples

Handshake and manifest:

```text
> HELLO
< {"hello":"mn42"}
> GET_MANIFEST
< {"device_name":"MOARkNOBS-42","schema_version":6,"slot_count":42,"pot_count":42,"envelope_count":6,"arg_method_count":14,"led_count":52,"power_profile":"POWER_CHOKED_V1","led_brightness_cap":26,"rail_topology_verified":false}
```

Set one live value:

```text
> SET_SLOT_VALUE,2,77
< {"type":"response","status":"ok"}
```

Set LED color under the active board cap:

```text
> SET_LED,26,0,255,128
< {"type":"response","status":"ok"}
```

Save and recall a scene:

```text
> {"cmd":"SAVE_SCENE","slot":0,"name":"Verse"}
< {"cmd":"SAVE_SCENE","scene_saved":true,"scene_slot":0,"scene_name":"Verse","scene_available":true}
> {"cmd":"RECALL_SCENE","slot":0}
< {"cmd":"RECALL_SCENE","scene_slot":0,"scene_name":"Verse","scene_available":true,"scene_recalled":true}
```
