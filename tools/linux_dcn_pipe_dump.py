#!/usr/bin/env python3
"""Read-only dump of the curated DCN 3.2 pipe register list on Linux (run as root, amdgpu loaded, monitor lit, text console).

    sudo python3 linux_dcn_pipe_dump.py                          # six safe groups into dcn_pipe_<pci device id>.txt
    sudo python3 linux_dcn_pipe_dump.py --out mydump.txt hpd_aux4 dig4_dp4 clocks otg0 opp0 mpc

Options: --out FILE (default dcn_pipe_<device>.txt), --reglist FILE (default dcn32_pipe_reglist.txt next to this script).
Every line is written to the output file and fsynced before the next register is read, and a group marker is written (and
fsynced) before the first register of each group, so if a read ever locks the machine the file shows the group and the last
register reached.

Groups, safest first: hpd_aux4 dig4_dp4 clocks otg0 opp0 mpc hubbub hubp0 dpp0 dcnvm dcio; for a monitor on DIG1 (e.g. HDMI on a Navi 33): hpd_aux1 dig1_hdmi. With no group names it reads the first six.
The list is for the DisplayPort path of the Navi 32 development board (HPD4/AUX4/DIG4): if your monitor is on another connector
the DIG4/HPD4 registers read as idle (DIG4_DIG_FE_CNTL 0). The DMU register bases are read from the kernel's IP discovery in sysfs
(hw_id 271) and fall back to Navi 32's.
"""
import glob
import os
import struct
import sys

NAVI32_BASES = {0: 0x12, 1: 0xC0, 2: 0x34C0, 3: 0x9000}   # DMU block bases of the development card (Navi 32)
DEFAULT = ["hpd_aux4", "dig4_dp4", "clocks", "otg0", "opp0", "mpc"]


def find_card():
    """Returns (debugfs dir, PCI device id string) of the first amdgpu card."""
    for d in sorted(glob.glob("/sys/kernel/debug/dri/*")):
        try:
            if "amdgpu" in open(d + "/name").read() and os.path.exists(d + "/amdgpu_regs"):
                dev = open(d + "/device/device").read().strip() if os.path.exists(d + "/device/device") else ""
                return d, dev
        except OSError:
            pass
    raise SystemExit("no amdgpu debugfs (amdgpu loaded? debugfs mounted? root?)")


def dmu_bases(dev):
    """DMU (hw_id 271) base addresses from /sys/class/drm/cardN/device/ip_discovery, or None."""
    for card in sorted(glob.glob("/sys/class/drm/card[0-9]*/device")):
        try:
            if open(card + "/device").read().strip() != dev:
                continue
            for inst in sorted(glob.glob(card + "/ip_discovery/die/0/*/[0-9]*")):
                if open(inst + "/hw_id").read().strip() == "271":
                    return {i: int(l, 16) for i, l in enumerate(open(inst + "/base_addr").read().split())}
        except (OSError, ValueError):
            pass
    return None


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    out_path = reglist = None
    groups = []
    while args:
        a = args.pop(0)
        if a in ("-h", "--help"):
            sys.exit(__doc__)
        elif a == "--out" and args:
            out_path = args.pop(0)
        elif a == "--reglist" and args:
            reglist = args.pop(0)
        elif a.startswith("-"):
            sys.exit("unknown option %s\n\n%s" % (a, __doc__))
        else:
            groups.append(a)
    if len(groups) >= 2 and groups[0].endswith(".txt") and groups[1].endswith(".txt"):   # the old form: REGLIST OUT [GROUP...]
        reglist, out_path, groups = groups[0], groups[1], groups[2:]
    card, dev = find_card()
    out_path = out_path or "dcn_pipe_%s.txt" % (dev or "unknown")
    regs = [l.split() for l in open(reglist or os.path.join(here, "dcn32_pipe_reglist.txt")) if l.strip()]
    groups = groups or DEFAULT
    bases = dmu_bases(dev)
    if bases is None:
        bases = NAVI32_BASES
        print("warning: no ip_discovery in sysfs, using Navi 32's DMU bases %s" % {i: hex(b) for i, b in bases.items()}, file=sys.stderr)
    elif bases != NAVI32_BASES:
        print("note: this card's DMU bases differ from Navi 32's: %s" % {i: hex(b) for i, b in bases.items()}, file=sys.stderr)
    fd = os.open(card + "/amdgpu_regs", os.O_RDONLY)
    out = open(out_path, "a")

    def emit(text):
        out.write(text + "\n")
        out.flush()
        os.fsync(out.fileno())

    emit("# device %s, DMU bases %s" % (dev, " ".join(hex(bases[i]) for i in sorted(bases))))
    for g in groups:
        emit(f"# group {g}")
        for grp, name, idx, off in regs:
            if grp != g:
                continue
            addr = (bases[int(idx)] + int(off, 16)) * 4
            v, = struct.unpack("<I", os.pread(fd, 4, addr))
            emit(f"{name} {idx} {off} 0x{v:08x}")
        emit(f"# done {g}")
    print("wrote %s" % out_path, file=sys.stderr)


if __name__ == "__main__":
    main()
