# Evidence handling and publication boundaries

## Publicly shareable in this document set

- PCI vendor/device identity and non-unique hardware revisions.
- Decoded IP Discovery versions, topology summaries and checksum results.
- VBIOS identification strings, decoded board/version fields and hashes.
- Sanitized firmware version inventory.
- Aggregated display trace event counts and derived refresh-rate summary.
- Capture date and explicit limitations.

## Keep local / do not publish raw

- Extracted VBIOS ROM and firmware binaries.
- Raw ACPI tables, including VFCT, DSDT and SSDT payloads.
- Raw PCI configuration dumps.
- Unsanitized GEM/VM/fence debugfs output that may expose process names, PIDs, BO handles or addresses.
- Raw kernel traces containing process names, PIDs, kernel pointers or detailed timing.
- Raw monitor EDID if it includes a serial number or other identifying data.
- Credentials, tokens, private keys or account data.

## Reproducibility

The local evidence archive contains capture scripts, decoded output and SHA-256 manifests. Public summaries should preserve capture date, hardware identity, relevant tool/kernel versions and the distinction between observed values and interpretation. A checksum identifies a file; it does not prove the file is safe to publish.

## Scope

This is Linux-side research on a physical Navi 33 device. Apple Silicon M1 Metal observations, if collected separately, are reference evidence for Apple's Metal stack and must not be presented as direct RX 7600 measurements.
