# Linux PCI resource parser adapter

This adapter consumes caller-provided Linux sysfs `resource` text; it does not access sysfs itself. It preserves start/end addresses, classifies IO versus memory using Linux `IORESOURCE_IO` (0x100) and `IORESOURCE_MEM` (0x200), and records the `IORESOURCE_PREFETCH` bit (0x2000). Empty zero rows are ignored. The parsed non-empty records are passed through `validatePciResources`.

Safety boundaries:
- No file opening, BAR mapping, register reads/writes, firmware loading, or device initialization.
- Unknown or conflicting flags fail closed.
- Output capacity and the validator's six-resource maximum are enforced.
- A parsed `enabled=true` means only that a non-empty sysfs resource line was supplied; it is not a claim that mapping is safe or that the device is active.
- Linux flags and ranges are not automatically a macOS resource representation.

The unit test includes the five non-empty resource lines captured from RX 7600 PCI function 0000:03:00.0, plus malformed/unsupported-input cases. Test data is a fixture copied from the read-only evidence file; the test does not read hardware.
