# 2025-10-28 Hardware fault

The emulator currently ignores IOT instructions sent to devices that are
not present. That behavior hides bugs in guest code and makes it harder to
diagnose misconfigured device wiring or missing device registrations.

Proposal
- Treat an IOT to an unregistered device as a hardware fault.
- On such a fault the emulator should:
	- stop CPU execution (halt) and set a visible emulator fault state;
	- emit a diagnostic log entry including the program counter (PC), the
		instruction word, and the device number; and
	- propagate an error to the Python wrapper (for example, raise an
		exception) so test harnesses and front-ends can detect the condition.

Why
- Failing fast and providing a clear diagnostic makes it easier to find
	misconfigured or buggy code that depends on absent devices.
- Tests and automation can detect and recover from these failures when the
	emulator reports them deterministically.

Acceptance test
- Add a unit/regression test that executes an IOT instruction whose device
	number is not present and asserts that the emulator halts (or raises) and
	that the diagnostic message contains PC and device info.

Implementation notes
- In the emulator IOT dispatch path, check whether the target device is
	registered. If not:
	- set an emulator fault code (for example, HW_FAULT);
	- log a message such as "HW_FAULT: PC=0o%06o INST=0o%06o DEV=%d";
	- halt the CPU and return an error from the run loop; and
	- map that condition to an exception in the Python wrapper.
- Add the acceptance test under `tests/` or in `factory/` and update this
	devlog entry once implemented.