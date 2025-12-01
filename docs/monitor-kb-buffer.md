# Monitor keyboard buffer (`kb` command)

- Syntax: `kb <text>` captures everything after the command (leading whitespace trimmed) and stores it in an internal buffer until delivered. Calling `kb` with no arguments shows buffer status (consumed/remaining counts and a short preview).
- Delivery: the monitor feeds one buffered character into the KL8E keyboard queue each time it services console input (during `go`, `run`, `c`, `t`, etc.), starting immediately after `kb` is issued.
- Translation: newline characters in the buffer are converted to carriage return before they are handed to the emulator; bytes are otherwise passed through and masked by the KL8E device.
- Break handling: buffered characters do not trigger the `.` user-break shortcut; only live keyboard input can stop execution in that way.
- Lifecycle: issuing another `kb` command replaces any pending buffer; the buffer is cleared automatically when all characters have been delivered or when the monitor shuts down.
