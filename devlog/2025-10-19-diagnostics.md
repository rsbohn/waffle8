# Checkpoint Diagnostics – 2025-10-19

Comparing `checkpoint/0000` and `checkpoint/0001` shows 24 divergent words, all in page 034 (octal addresses 7006–7063). The newer snapshot mirrors `demo/core.srec` across the core dispatch table and KL8E handler block, while `0000` still carries zeroes/old values in the same regions. Conclusion: `0000` was captured before reloading the latest core; treat it as stale and prefer `0001` (or regenerate) for future work.
