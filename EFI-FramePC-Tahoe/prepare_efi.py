#!/usr/bin/env python3
"""Assemble the local, non-installed FramePC OpenCore staging EFI."""
from __future__ import annotations

import plistlib
import json
import re
import shutil
import uuid
from pathlib import Path


ROOT = Path(__file__).resolve().parent
DOWNLOADS = ROOT / "_downloads"
EFI = ROOT / "EFI"
OC = EFI / "OC"
PLATFORM_PROFILE = ROOT / "smbios.local.json"


def copy_tree(source: Path, destination: Path) -> None:
    if not source.is_dir():
        raise FileNotFoundError(source)
    shutil.copytree(source, destination)


def add_kext(config: dict, bundle: Path, comment: str) -> None:
    info_path = bundle / "Contents" / "Info.plist"
    info = plistlib.loads(info_path.read_bytes())
    executable = info.get("CFBundleExecutable")
    if not executable or not (bundle / "Contents" / "MacOS" / executable).is_file():
        raise FileNotFoundError(f"Missing kext executable in {bundle}")
    config["Kernel"]["Add"].append(
        {
            "Arch": "x86_64",
            "BundlePath": bundle.name,
            "Comment": comment,
            "Enabled": True,
            "ExecutablePath": f"Contents/MacOS/{executable}",
            "MaxKernel": "",
            "MinKernel": "",
            "PlistPath": "Contents/Info.plist",
        }
    )


def main() -> None:
    config_path = OC / "config.plist"
    if EFI.exists():
        raise FileExistsError(f"Refusing to overwrite existing EFI tree {EFI}")

    profile = json.loads(PLATFORM_PROFILE.read_text(encoding="utf-8-sig"))
    serial = profile.get("SystemSerialNumber", "")
    mlb = profile.get("MLB", "")
    rom_hex = profile.get("ROM", "")
    product = profile.get("SystemProductName", "")
    if not serial or not mlb or not product or not re.fullmatch(r"[0-9A-Fa-f]{12}", rom_hex):
        raise ValueError("smbios.local.json must provide product, serial, MLB, and a 12-digit ROM MAC")

    for directory in (EFI / "BOOT", OC / "ACPI", OC / "Drivers", OC / "Kexts"):
        directory.mkdir(parents=True, exist_ok=False)

    opencore = DOWNLOADS / "OpenCorePkg"
    x64 = opencore / "X64" / "EFI"
    shutil.copy2(x64 / "BOOT" / "BOOTx64.efi", EFI / "BOOT" / "BOOTx64.efi")
    shutil.copy2(x64 / "OC" / "OpenCore.efi", OC / "OpenCore.efi")
    shutil.copy2(x64 / "OC" / "Drivers" / "OpenRuntime.efi", OC / "Drivers" / "OpenRuntime.efi")
    shutil.copy2(x64 / "OC" / "Drivers" / "OpenHfsPlus.efi", OC / "Drivers" / "OpenHfsPlus.efi")
    shutil.copy2(x64 / "OC" / "Drivers" / "ResetNvramEntry.efi", OC / "Drivers" / "ResetNvramEntry.efi")
    shutil.copy2(
        DOWNLOADS / "SSDT-EC-USBX-DESKTOP.aml",
        OC / "ACPI" / "SSDT-EC-USBX-DESKTOP.aml",
    )

    bundles = [
        (DOWNLOADS / "Lilu" / "Lilu.kext", "Lilu patching framework"),
        (DOWNLOADS / "VirtualSMC" / "Kexts" / "VirtualSMC.kext", "SMC emulation"),
        (DOWNLOADS / "WhateverGreen" / "WhateverGreen.kext", "Graphics compatibility framework (does not add Navi 33 support)"),
        (DOWNLOADS / "AppleALC" / "AppleALC.kext", "Audio codec framework; layout not selected yet"),
        (DOWNLOADS / "NVMeFix" / "NVMeFix.kext", "NVMe power management"),
        (
            DOWNLOADS / "RTL8111_driver_for_OS_X" / "RealtekRTL8111-V3.0.0" / "Release" / "RealtekRTL8111.kext",
            "Realtek RTL8111/8168 Ethernet",
        ),
    ]

    sample = plistlib.loads((DOWNLOADS / "OpenCore-Sample.plist").read_bytes())
    patches = plistlib.loads((DOWNLOADS / "AMD-Vanilla-patches.plist").read_bytes())
    config = sample

    config["ACPI"]["Add"] = [
        {
            "Comment": "Dortania desktop EC and USB power properties",
            "Enabled": True,
            "Path": "SSDT-EC-USBX-DESKTOP.aml",
        }
    ]
    config["Booter"]["Quirks"].update(
        {
            "DevirtualiseMmio": False,
            "EnableWriteUnprotector": False,
            "RebuildAppleMemoryMap": True,
            "ResizeAppleGpuBars": -1,
            "SetupVirtualMap": True,
            "SyncRuntimePermissions": True,
        }
    )
    config["Kernel"]["Add"] = []
    config["Kernel"]["Patch"] = patches["Kernel"]["Patch"]
    config["Kernel"]["Quirks"]["ProvideCurrentCpuInfo"] = True
    config["Kernel"]["Quirks"]["XhciPortLimit"] = False

    core_replacements = (
        bytes.fromhex("B8 06 00 00 00 00"),
        bytes.fromhex("BA 06 00 00 00 00"),
        bytes.fromhex("BA 06 00 00 00 90"),
        bytes.fromhex("BA 06 00 00 00"),
    )
    core_patch_index = 0
    for patch in config["Kernel"]["Patch"]:
        if "Force cpuid_cores_per_package" in patch.get("Comment", ""):
            patch["Replace"] = core_replacements[core_patch_index]
            core_patch_index += 1
    if core_patch_index != 4:
        raise ValueError(f"Expected four AMD core-count patches, found {core_patch_index}")

    config["Misc"]["Boot"]["ShowPicker"] = True
    config["Misc"]["Boot"]["Timeout"] = 5
    config["Misc"]["Security"].update(
        {"ScanPolicy": 0, "SecureBootModel": "Default", "Vault": "Optional"}
    )
    config["NVRAM"]["Add"]["7C436110-AB2A-4BBB-A880-FE41995C9F82"].update(
        {
            "boot-args": "-v keepsyms=1 debug=0x100",
            "prev-lang:kbd": b"en-US:0",
            "run-efi-updater": "No",
            "csr-active-config": bytes.fromhex("00 00 00 00"),
        }
    )

    generic = config["PlatformInfo"]["Generic"]
    generic.update(
        {
            "MLB": mlb,
            "ROM": bytes.fromhex(rom_hex),
            "SystemProductName": product,
            "SystemSerialNumber": serial,
            "SystemUUID": str(uuid.uuid4()).upper(),
        }
    )

    config["UEFI"]["Drivers"] = [
        {
            "Arguments": "",
            "Comment": "OpenCore runtime services",
            "Enabled": True,
            "HideVerbose": False,
            "LoadEarly": False,
            "Path": "OpenRuntime.efi",
        },
        {
            "Arguments": "",
            "Comment": "Open HFS Plus filesystem support",
            "Enabled": True,
            "HideVerbose": True,
            "LoadEarly": False,
            "Path": "OpenHfsPlus.efi",
        },
        {
            "Arguments": "",
            "Comment": "OpenCore NVRAM reset picker entry",
            "Enabled": True,
            "HideVerbose": False,
            "LoadEarly": False,
            "Path": "ResetNvramEntry.efi",
        },
    ]
    config["UEFI"]["Quirks"]["RequestBootVarRouting"] = True

    for source, comment in bundles:
        destination = OC / "Kexts" / source.name
        copy_tree(source, destination)
        add_kext(config, destination, comment)

    with config_path.open("wb") as stream:
        plistlib.dump(config, stream, fmt=plistlib.FMT_XML, sort_keys=False)
    print(f"Created {config_path}")
    print(f"Added {len(config['Kernel']['Add'])} kexts and {len(config['Kernel']['Patch'])} AMD kernel patches")
    print("Core-count replacements set to six physical cores; per-machine SMBIOS profile applied.")


if __name__ == "__main__":
    main()
