# native-actors vs napa-drive-v5 probe (2026-10-12, read-only)

Ran `./build/oracle-scene-test --native-actors
analysis/oracle/napa-drive-v5.tsv` (build at `36ca5bc` + NodeCompare's
uncommitted oracle_scene rewrite):

- frame 0: 5 actor nodes match
- frame 150: 6 actor nodes match
- frame 300: 6 actor nodes match
- frame 750: MISMATCH view point 0 pos 26439 (checked 1)

4 frames, 17 actor nodes, 1 failure. No files touched; NodeCompare owns
`tests/oracle_scene_test.cpp` + `oracle/aweman_proxy.c`.
