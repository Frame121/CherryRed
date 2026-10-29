#!/usr/bin/env python3
"""Read-only dump of the curated DCN 3.2 pipe register list on Linux (run as root).

Every line is written to the output file and fsynced before the next register is read, and a group marker is
written (and fsynced) before the first register of each group, so if a read ever locks the machine the file
shows the group and the last register reached. Run with the monitor lit by amdgpu.

    sudo python3 linux_dcn_pipe_dump.py dcn32_pipe_reglist.txt dcn_pipe_linux.txt hpd_aux4 dig4_dp4 clocks otg0 opp0 mpc

Groups, safest first: hpd_aux4 dig4_dp4 clocks otg0 opp0 mpc hubbub hubp0 dpp0 dcnvm dcio. With no group names it reads the first six.
"""
import glob
import os
import struct
import sys

BASES = {0: 0x12, 1: 0xC0, 2: 0x34C0, 3: 0x9000}   # DMU block bases from this card's IP discovery table
DEFAULT = ["hpd_aux4", "dig4_dp4", "clocks", "otg0", "opp0", "mpc"]


def find_card():
    for d in sorted(glob.glob("/sys/kernel/debug/dri/*")):
        try:
            if "amdgpu" in open(d + "/name").read() and os.path.exists(d + "/amdgpu_regs"):
                dev = open(d + "/device/device").read().strip() if os.path.exists(d + "/device/device") else ""
                if dev in ("", "0x747e", "0x7480"):   # 0x7480 = Navi 33; the DMU bases below are Navi 32's, check them against linux_ip_discovery.py
                    return d
        except OSError:
            pass
    raise SystemExit("no amdgpu debugfs for 1002:747e or 1002:7480 (amdgpu loaded? debugfs mounted? root?)")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    groups = sys.argv[3:] or DEFAULT
    regs = [l.split() for l in open(sys.argv[1]) if l.strip()]
    fd = os.open(find_card() + "/amdgpu_regs", os.O_RDONLY)
    out = open(sys.argv[2], "a")

    def emit(text):
        out.write(text + "\n")
        out.flush()
        os.fsync(out.fileno())

    for g in groups:
        emit(f"# group {g}")
        for grp, name, idx, off in regs:
            if grp != g:
                continue
            addr = (BASES[int(idx)] + int(off, 16)) * 4
            v, = struct.unpack("<I", os.pread(fd, 4, addr))
            emit(f"{name} {idx} {off} 0x{v:08x}")
        emit(f"# done {g}")


if __name__ == "__main__":
    main()
