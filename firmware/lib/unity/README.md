# Unity Config Override

We don't let upstream's `unity_config.c` boss us around. The stock file yells straight at `Serial`, which jams up host runs and ignores our redirect shim.

This stub pipes Unity's output through our `unityTest*` wrappers so both the Teensy and the host stay in tune. Drop any future tweaks here.

