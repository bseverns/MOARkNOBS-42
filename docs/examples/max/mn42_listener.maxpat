{
    "patcher": {
        "fileversion": 1,
        "rect": [0.0, 0.0, 600.0, 400.0],
        "bglocked": 0,
        "defrect": [0.0, 0.0, 600.0, 400.0],
        "openrect": [0.0, 0.0, 600.0, 400.0],
        "gridsnaponcreate": 1,
        "toolbarvisible": 1,
        "boxes": [
            {"box": {"id": "comment1", "maxclass": "comment", "text": "listen on UDP 9000, scream the slot data", "patching_rect": [50.0, 10.0, 500.0, 20.0]}},
            {"box": {"id": "obj1", "maxclass": "newobj", "text": "udpreceive 9000", "patching_rect": [50.0, 50.0, 110.0, 20.0]}},
            {"box": {"id": "obj2", "maxclass": "newobj", "text": "oscparse", "patching_rect": [50.0, 80.0, 70.0, 20.0]}},
            {"box": {"id": "obj3", "maxclass": "newobj", "text": "print slot", "patching_rect": [50.0, 110.0, 80.0, 20.0]}},
            {"box": {"id": "comment2", "maxclass": "comment", "text": "want to poke back?", "patching_rect": [250.0, 10.0, 300.0, 20.0]}},
            {"box": {"id": "message1", "maxclass": "message", "text": "/slot/0/value 0.5", "patching_rect": [250.0, 50.0, 130.0, 20.0]}},
            {"box": {"id": "obj4", "maxclass": "newobj", "text": "oscformat", "patching_rect": [250.0, 80.0, 70.0, 20.0]}},
            {"box": {"id": "obj5", "maxclass": "newobj", "text": "udpsend localhost 9001", "patching_rect": [250.0, 110.0, 170.0, 20.0]}},
            {"box": {"id": "comment3", "maxclass": "comment", "text": "MIDI in -> out", "patching_rect": [250.0, 150.0, 120.0, 20.0]}},
            {"box": {"id": "obj6", "maxclass": "newobj", "text": "ctlin", "patching_rect": [250.0, 180.0, 40.0, 20.0]}},
            {"box": {"id": "obj7", "maxclass": "newobj", "text": "ctlout", "patching_rect": [250.0, 210.0, 50.0, 20.0]}}
        ],
        "lines": [
            {"patchline": {"source": ["obj1", 0], "destination": ["obj2", 0]}},
            {"patchline": {"source": ["obj2", 0], "destination": ["obj3", 0]}},
            {"patchline": {"source": ["message1", 0], "destination": ["obj4", 0]}},
            {"patchline": {"source": ["obj4", 0], "destination": ["obj5", 0]}},
            {"patchline": {"source": ["obj6", 0], "destination": ["obj7", 0]}}
        ]
    }
}
