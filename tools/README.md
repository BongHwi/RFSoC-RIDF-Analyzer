# Tools

This directory contains operational helper scripts.

## `tmux_rf.sh`
- Connects to `rf01`~`rf05` over SSH, runs `init.sh`, then starts `rfdcbabies.elf` with per-host arguments.
- Creates a `tmux` session (`rf_deployment`) and arranges one pane per server in a tiled layout.
- If the same session already exists, it asks whether to kill it or cancel.

Run:
```bash
./tools/tmux_rf.sh
```

## `update_rfsoc_conf.sh`
- Takes Delay (ns) and Data Count (word), then rounds/adjusts them to FPGA resolution units (16 ns, 8 words).
- Converts adjusted values to hex (`dl`, `len`) and applies them to `bin/fpgainit.sh` on `rf01`~`rf05`.
- Prints computed values first, then applies only after user confirmation (`y`).

Run:
```bash
./tools/update_rfsoc_conf.sh <Delay_ns> <Data_Count_word>
```

Example:
```bash
./tools/update_rfsoc_conf.sh 250 130
```

## Notes
- Ensure `tmux` is installed on the local machine before running `tmux_rf.sh`.
- Ensure SSH access to `rf01`~`rf05` is configured (keys, host aliases, and permissions).
- Verify that `bin/init.sh`, `bin/rfdcbabies.elf`, and `bin/fpgainit.sh` exist on target hosts.
- `update_rfsoc_conf.sh` modifies remote files in place with `sed -i`; review values carefully before confirming.
- Run these scripts only in environments where applying settings to all five RF servers is intended.
