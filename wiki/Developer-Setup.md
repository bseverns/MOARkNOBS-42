# Developer Setup

This page takes a contributor from a source clone to a firmware handshake,
local App, and optional Bridge.
Canonical source: `docs/project/ProcessOverview.md`

![Pipeline graphic showing install, build, HELLO handshake, WebSerial app connection, and optional bridge setup.](assets/workflows/getting-started-pipeline.png)

## Prerequisites

- PlatformIO Core on PATH (`pio`)
- Node.js 24.x for Bridge/App tooling
- Python 3.11 or newer for repository helper scripts
- Teensy 4.0 connected over USB

## 1. Install dependencies

From repository root:

```bash
pip install -r requirements.txt
npm --prefix bridge ci
npm --prefix App ci
```

## 2. Build and upload firmware

The PlatformIO project root is `firmware/`:

```bash
pio run -d firmware -e teensy40_main
pio run -d firmware -e teensy40_main -t upload
```

## 3. Verify the serial handshake

Open a serial terminal and send:

```text
HELLO
```

Expected response includes:

```json
{ "hello": "mn42" }
```

## 4. Run the App locally

```bash
python3 -m http.server -d App
```

Open `http://localhost:8000/`, click **Connect**, and confirm manifest data
appears.

## 5. Run the Bridge when needed

```bash
node bridge/mn42_bridge.js --serial /dev/ttyACM0 --osc 9000 --osc-listen 9000 --host 127.0.0.1 --bind 127.0.0.1 --midi "MN42 Bridge"
```

Replace `/dev/ttyACM0` with the actual device path.

## Contributor routes

- `docs/project/ProcessOverview.md`
- `docs/getting-started/BuildersHandbook.md`
- `docs/validation/TESTING.md`
- [Wiki testing map](Testing.md)
- [Symptom-based troubleshooting](Troubleshooting.md)
