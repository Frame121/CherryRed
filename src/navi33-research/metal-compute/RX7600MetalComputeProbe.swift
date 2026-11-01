import Foundation
import Metal
import IOKit
import CoreFoundation

private let targetVendor: UInt32 = 0x1002
private let targetDevice: UInt32 = 0x7480
private let targetSubVendor: UInt32 = 0x148c
private let targetSubDevice: UInt32 = 0x2421

func fail(_ message: String, code: Int32 = 1) -> Never {
    fputs("CherryRed RX7600 Metal: ERROR: \(message)\n", stderr)
    exit(code)
}

func readPCIWord(_ entry: io_registry_entry_t, _ key: String) -> UInt32? {
    guard let value = IORegistryEntryCreateCFProperty(
        entry, key as CFString, kCFAllocatorDefault, 0)?.takeRetainedValue() else {
        return nil
    }
    if let data = value as? Data, data.count >= 4 {
        return UInt32(data[0]) | (UInt32(data[1]) << 8) |
               (UInt32(data[2]) << 16) | (UInt32(data[3]) << 24)
    }
    if let number = value as? NSNumber { return number.uint32Value }
    return nil
}

struct PCIMatch {
    let vendor: UInt32
    let device: UInt32
    let subVendor: UInt32
    let subDevice: UInt32
    let matchedEntry: String
}

func findPCIAncestor(for registryID: UInt64) -> PCIMatch? {
    var iterator: io_iterator_t = 0
    guard let matching = IOServiceMatching("IOAccelerator"),
          IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) == KERN_SUCCESS else {
        return nil
    }
    defer { IOObjectRelease(iterator) }

    var service = IOIteratorNext(iterator)
    while service != 0 {
        defer { IOObjectRelease(service) }
        var serviceID: UInt64 = 0
        if IORegistryEntryGetRegistryEntryID(service, &serviceID) == KERN_SUCCESS,
           serviceID == registryID {
            var current = service
            var depth = 0
            while current != 0 && depth < 16 {
                let vendor = readPCIWord(current, "vendor-id")
                let device = readPCIWord(current, "device-id")
                if let vendor, let device {
                    let subVendor = readPCIWord(current, "subsystem-vendor-id") ?? 0
                    let subDevice = readPCIWord(current, "subsystem-id") ?? 0
                    return PCIMatch(vendor: vendor, device: device,
                                    subVendor: subVendor, subDevice: subDevice,
                                    matchedEntry: "IOAccelerator PCI ancestor")
                }
                var parent: io_registry_entry_t = 0
                guard IORegistryEntryGetParentEntry(current, kIOServicePlane, &parent) == KERN_SUCCESS else {
                    break
                }
                if current != service { IOObjectRelease(current) }
                current = parent
                depth += 1
            }
            if current != 0 && current != service { IOObjectRelease(current) }
        }
        service = IOIteratorNext(iterator)
    }
    return nil
}

let devices = MTLCopyAllDevices()
guard !devices.isEmpty else {
    fail("MTLCopyAllDevices() returned no devices. RX 7600 acceleration is not available.", code: 2)
}

var matchedDevice: MTLDevice?
for device in devices {
    print("Metal device candidate: \(device.name), registryID=0x\(String(device.registryID, radix: 16))")
    guard let pci = findPCIAncestor(for: device.registryID) else {
        print("  PCI ancestry: unavailable; this device will not be treated as the RX 7600.")
        continue
    }
    print(String(format: "  PCI: %04x:%04x subsystem %04x:%04x (%@)",
                 pci.vendor, pci.device, pci.subVendor, pci.subDevice, pci.matchedEntry))
    if pci.vendor == targetVendor && pci.device == targetDevice &&
       pci.subVendor == targetSubVendor && pci.subDevice == targetSubDevice {
        matchedDevice = device
    }
}

guard let device = matchedDevice else {
    fail("No Metal device maps to exact RX 7600 PCI identity 1002:7480 / 148c:2421. No other GPU will be accepted.", code: 3)
}

let shaderURL = URL(fileURLWithPath: CommandLine.arguments.count > 1
    ? CommandLine.arguments[1] : "build/navi33-metal-compute/CherryRedCompute.metallib")

do {
    let library = try device.makeLibrary(URL: shaderURL)
    guard let function = library.makeFunction(name: "cherryred_vector_add") else {
        fail("Metal library is missing cherryred_vector_add")
    }
    let pipeline = try device.makeComputePipelineState(function: function)
    let count = 4096
    let bytes = count * MemoryLayout<Float>.stride
    guard let lhs = device.makeBuffer(length: bytes, options: .storageModeShared),
          let rhs = device.makeBuffer(length: bytes, options: .storageModeShared),
          let output = device.makeBuffer(length: bytes, options: .storageModeShared) else {
        fail("RX 7600 Metal buffer allocation failed")
    }
    let a = lhs.contents().bindMemory(to: Float.self, capacity: count)
    let b = rhs.contents().bindMemory(to: Float.self, capacity: count)
    let c = output.contents().bindMemory(to: Float.self, capacity: count)
    for i in 0..<count {
        a[i] = Float(i) * 0.25
        b[i] = Float(count - i) * 0.5
        c[i] = .nan
    }
    guard let queue = device.makeCommandQueue(),
          let command = queue.makeCommandBuffer(),
          let encoder = command.makeComputeCommandEncoder() else {
        fail("RX 7600 Metal command objects could not be created")
    }
    var n = UInt32(count)
    encoder.setComputePipelineState(pipeline)
    encoder.setBuffer(lhs, offset: 0, index: 0)
    encoder.setBuffer(rhs, offset: 0, index: 1)
    encoder.setBuffer(output, offset: 0, index: 2)
    encoder.setBytes(&n, length: MemoryLayout<UInt32>.size, index: 3)
    let width = max(1, pipeline.threadExecutionWidth)
    encoder.dispatchThreadgroups(
        MTLSize(width: (count + width - 1) / width, height: 1, depth: 1),
        threadsPerThreadgroup: MTLSize(width: width, height: 1, depth: 1))
    encoder.endEncoding()
    command.commit()
    command.waitUntilCompleted()
    if let error = command.error { fail("GPU command failed: \(error)") }
    for i in 0..<count {
        let expected = Float(i) * 0.25 + Float(count - i) * 0.5
        if !c[i].isFinite || abs(c[i] - expected) > 0.0001 {
            fail("GPU result mismatch at element \(i): \(c[i]) != \(expected)")
        }
    }
    print("PASS: exact RX 7600 hardware executed and verified \(count) Metal vector additions.")
} catch {
    fail("Metal pipeline/dispatch failed: \(error)")
}
