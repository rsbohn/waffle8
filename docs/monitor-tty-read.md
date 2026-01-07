# Monitor tty read (`tty read` command)

- Syntax: `tty read <file>` loads the file contents into the KL8E keyboard buffer and appends an EOT (0x04).
- Delivery: buffered characters are streamed to the KL8E keyboard queue on each console service (same as `kb`).
- Translation: newline bytes are converted to carriage return before delivery; other bytes pass through unchanged.
- Lifecycle: issuing another `tty read` or `kb` replaces any pending buffer.
