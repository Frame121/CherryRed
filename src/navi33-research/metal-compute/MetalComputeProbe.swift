import Foundation
import Metal

func fail(_ message: String, code: Int32 = 1) -> Never {
    fputs("CherryRed Metal probe: ERROR: \(message)\n", stderr)
    exit(code)
}

let args = CommandLine.arguments
 guard args.count == 2 else {
    fail("usage: MetalComputeProbe <path-to-metallib>")
}
let libraryURL = URL(fileURLWithPath: args[1])

guard let device = MTLCreateSystemDefaultDevice() else {
    fail("MTLCreateSystemDefaultDevice() returned nil; no Metal device is available. No CPU fallback is used.", code: 2)
}
print("Metal device: \(device.name)")

do {
    let library = try device.makeLibrary(URL: libraryURL)
    guard let function = library.makeFunction(name: "cherryred_vector_add") else {
        fail("kernel cherryred_vector_add not found in metallib")
    }
    let pipeline = try device.makeComputePipelineState(function: function)

    let count = 4096
    let byteCount = count * MemoryLayout<Float>.stride
    guard let lhs = device.makeBuffer(length: byteCount, options: .storageModeShared),
          let rhs = device.makeBuffer(length: byteCount, options: .storageModeShared),
          let output = device.makeBuffer(length: byteCount, options: .storageModeShared) else {
        fail("could not allocate shared Metal buffers")
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
          let commandBuffer = queue.makeCommandBuffer(),
          let encoder = commandBuffer.makeComputeCommandEncoder() else {
        fail("could not create command queue / command buffer / encoder")
    }

    var elementCount = UInt32(count)
    encoder.setComputePipelineState(pipeline)
    encoder.setBuffer(lhs, offset: 0, index: 0)
    encoder.setBuffer(rhs, offset: 0, index: 1)
    encoder.setBuffer(output, offset: 0, index: 2)
    encoder.setBytes(&elementCount, length: MemoryLayout<UInt32>.size, index: 3)

    let width = max(1, pipeline.threadExecutionWidth)
    let groups = (count + width - 1) / width
    encoder.dispatchThreadgroups(
        MTLSize(width: groups, height: 1, depth: 1),
        threadsPerThreadgroup: MTLSize(width: width, height: 1, depth: 1))
    encoder.endEncoding()
    commandBuffer.commit()
    commandBuffer.waitUntilCompleted()

    if let error = commandBuffer.error {
        fail("GPU command buffer failed: \(error)")
    }
    for i in 0..<count {
        let expected = Float(i) * 0.25 + Float(count - i) * 0.5
        if !c[i].isFinite || abs(c[i] - expected) > 0.0001 {
            fail("verification failed at index \(i): got \(c[i]), expected \(expected)")
        }
    }
    print("PASS: \(count) vector additions verified on Metal device '\(device.name)'.")
} catch {
    fail("Metal setup/execution failed: \(error)")
}
