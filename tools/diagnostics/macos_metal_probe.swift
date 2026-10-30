#!/usr/bin/swift
import Foundation
import Metal

// Read-only Metal capability probe for CherryRed.
// Uses public Metal APIs only; it does not access PCI BARs/registers,
// load kernel extensions, or alter system settings.

func formatBytes(_ value: UInt64) -> String {
    let formatter = ByteCountFormatter()
    formatter.countStyle = .memory
    return formatter.string(fromByteCount: Int64(clamping: value))
}

print("CherryRed Metal capability probe")
print("Date: \(ISO8601DateFormatter().string(from: Date()))")
print("macOS: \(ProcessInfo.processInfo.operatingSystemVersionString)")
print("")

let devices = MTLCopyAllDevices()
print("Metal devices visible: \(devices.count)")

if devices.isEmpty {
    print("RESULT: No Metal devices were returned by MTLCopyAllDevices().")
    print("INTERPRETATION: Metal acceleration is not available through this API on this boot.")
    print("LIMIT: This does not identify every possible cause or test GPU initialization.")
}

for (index, device) in devices.enumerated() {
    print("")
    print("[Device \(index)]")
    print("Name: \(device.name)")
    print("Registry ID: 0x\(String(device.registryID, radix: 16))")
    print("Low power: \(device.isLowPower)")
    print("Headless: \(device.isHeadless)")
    print("Removable: \(device.isRemovable)")
    print("Unified memory: \(device.hasUnifiedMemory)")
    print("Recommended working set: \(formatBytes(device.recommendedMaxWorkingSetSize))")
    print("Maximum buffer length: \(formatBytes(UInt64(device.maxBufferLength)))")
    print("Supports ray tracing: \(device.supportsRaytracing)")
}

if let defaultDevice = MTLCreateSystemDefaultDevice() {
    print("")
    print("Default Metal device: \(defaultDevice.name)")
} else {
    print("")
    print("Default Metal device: none")
}

print("")
print("Safety: public Metal enumeration only; no hardware initialization or writes.")
