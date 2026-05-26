[bridge-session:server] bridge console: http://127.0.0.1:8791/
[bridge-session:server] serial up on /dev/cu.usbmodem192460701 @115200
[bridge-session] wrote report to logs/bridge-session-test.json
[bridge-session] PASS – Bridge server starts and exposes the console address: http://127.0.0.1:8791
[bridge-session] PASS – Structured bridge session becomes ready on hardware: schemaSource=device fw=0.0.0
[bridge-session] PASS – Structured websocket emits device.ready: device.ready observed on /ws/events
[bridge-session] PASS – Structured stage endpoint accepts a live device config: idle_floor 24 -> 25
[bridge-session] PASS – Structured apply returns ACK and promotes staged config: checksum=159c3066af5379ccec0553fbe5362f37ea2bf1bcd48f671cd1f2f890c8f60353
[bridge-session] PASS – Cleanup apply restores the original live config: idle_floor restored to 24
