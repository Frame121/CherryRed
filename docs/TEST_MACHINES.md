# Test machines

## Primary development machine

| Item | Value |
|---|---|
| Board | ASUS ROG STRIX Z490-G GAMING, BIOS 3001 |
| CPU | Intel Core i7-10700K (Comet Lake) |
| RAM | 16 GB |
| Primary display | Intel UHD 630, `8086:9BC5` |
| Target GPU | Sapphire Radeon RX 7800 XT, `1002:747E`, subsystem `1DA2:D475`, rev `C8` |
| Link | PCIe 4.0 x16 (bus 3, device 0, function 0 on Windows) |
| Windows driver | 32.0.31041.1004 |

macOS is booted on this machine through OpenCore with the UHD 630 driving the screen, so the 7800 XT is free for experiments and panics can be read.

## To collect before running anything on macOS

- [x] Above 4G Decoding / Resizable BAR state in the BIOS: Resizable BAR ON. VRAM is 16 GB.
- [ ] VBIOS dump (GPU-Z)
- [ ] IP discovery from a Linux live boot (`dmesg | grep -i discovery`, `/sys/kernel/debug/dri/*/`)
- [ ] ACPI VFCT table presence
