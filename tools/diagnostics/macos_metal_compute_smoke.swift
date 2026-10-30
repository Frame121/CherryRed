#!/usr/bin/swift
import Foundation
import Metal

private let shaderSource = """
#include <metal_stdlib>
using namespace metal;

kernel void addOne(device uint *values [[buffer(0)]],
                   uint index [[thread_position_in_grid]]) {
    values[index] += 1;
}
"""

func runComputeSmokeTest(on device: MTLDevice) throws -> Bool {
    let itemCount = 4
    let byteCount = MemoryLayout<UInt32>.stride * itemCount

    guard let buffer = device.makeBuffer(length: byteCount, options: .storageModeShared) else {
        fputs("FAIL: could not allocate a shared Metal buffer.\n", stderr)
        return false
    }
    let values = buffer.contents().bindMemory(to: UInt32.self, capacity: itemCount)
    for index in 0..<itemCount {
        values[index] = UInt32(index)
    }

    let library = try device.makeLibrary(source: shaderSource, options: nil)
    guard let function = library.makeFunction(name: "addOne") else {
        fputs("FAIL: Metal library did not contain addOne.\n", stderr)
        return false
    }
    let pipeline = try device.makeComputePipelineState(function: function)

    guard let queue = device.makeCommandQueue(),
          let commandBuffer = queue.makeCommandBuffer(),
          let encoder = commandBuffer.makeComputeCommandEncoder() else {
        fputs("FAIL: could not create the Metal command objects.\n", stderr)
        return false
    }

    encoder.setComputePipelineState(pipeline)
    encoder.setBuffer(buffer, offset: 0, index: 0)
    let grid = MTLSize(width: itemCount, height: 1, depth: 1)
    let group = MTLSize(
        width: min(itemCount, pipeline.maxTotalThreadsPerThreadgroup),
        height: 1,
        depth: 1
    )
    encoder.dispatchThreads(grid, threadsPerThreadgroup: group)
    encoder.endEncoding()
    commandBuffer.commit()
    commandBuffer.waitUntilCompleted()

    guard commandBuffer.status == .completed else {
        fputs("FAIL: Metal command buffer status is \(commandBuffer.status).\n", stderr)
        if let error = commandBuffer.error {
            fputs("Metal error: \(error.localizedDescription)\n", stderr)
        }
        return false
    }

    let actual = (0..<itemCount).map { values[$0] }
    let expected = (1...itemCount).map(UInt32.init)
    guard actual == expected else {
        fputs("FAIL: compute output \(actual), expected \(expected).\n", stderr)
        return false
    }

    print("PASS: Metal compute dispatch and readback succeeded.")
    print("Result: \(actual)")
    return true
}

print("CherryRed Metal compute smoke test")
print("macOS: \(ProcessInfo.processInfo.operatingSystemVersionString)")
let devices = MTLCopyAllDevices()
print("Metal devices visible: \(devices.count)")

guard !devices.isEmpty else {
    print("BLOCKED: macOS exposed no Metal device; no compute dispatch was possible.")
    print("This utility cannot create or install a GPU driver.")
    exit(2)
}

var allPassed = true
for (index, device) in devices.enumerated() {
    print("")
    print("[Device \(index)] \(device.name) (registry 0x\(String(device.registryID, radix: 16)))")
    do {
        allPassed = try runComputeSmokeTest(on: device) && allPassed
    } catch {
        fputs("FAIL: Metal setup or shader execution threw: \(error.localizedDescription)\n", stderr)
        allPassed = false
    }
}

if !allPassed {
    exit(1)
}
print("")
print("PASS: every Metal device completed the compute smoke test.")
