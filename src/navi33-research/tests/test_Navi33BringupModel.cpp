#include "Navi33BuildConfiguration.hpp"
#include "Navi33BringupModel.hpp"

#include <cstdio>
#include <limits>

using namespace cobalt64::research;

namespace {
int failures = 0;
int checks = 0;

void check(bool condition, const char* label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", label);
    }
}

void testFirmwareMetadataGate() {
    FirmwareDescriptor firmware{Navi33FirmwareEngine::GC, "gc_11_0_1.bin",
        4096, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
        FirmwareProvenance::BoardAttributed, FirmwareLicense::Approved};
    check(validateFirmwareDescriptor(firmware).valid,
          "complete firmware metadata passes structural gate");
    firmware.license = FirmwareLicense::ReviewRequired;
    check(validateFirmwareDescriptor(firmware).error ==
              ContractError::LicenseNotApproved,
          "firmware without license approval is rejected");
    firmware.license = FirmwareLicense::Approved;
    firmware.byteLength = 0;
    check(validateFirmwareDescriptor(firmware).error == ContractError::InvalidSize,
          "empty firmware image is rejected");
    firmware.byteLength = 4096;
    firmware.sha256 = "not-a-digest";
    check(validateFirmwareDescriptor(firmware).error == ContractError::InvalidDigest,
          "malformed digest is rejected");
    firmware.sha256 = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    firmware.provenance = FirmwareProvenance::Unknown;
    check(validateFirmwareDescriptor(firmware).error ==
              ContractError::UnknownProvenance,
          "unknown firmware provenance is rejected");
}

void testAddressRangeAndAllocator() {
    const GpuAddressRange aperture{0x100000, 0x10000};
    check(containsRange(aperture, {0x101000, 0x2000}),
          "contained GPU virtual range is accepted");
    check(!containsRange(aperture, {0x10f000, 0x2000}),
          "range extending beyond aperture is rejected");
    check(!containsRange({UINT64_MAX - 8, 32}, {UINT64_MAX - 4, 4}),
          "overflowing aperture is rejected");

    GpuVirtualAddressAllocator allocator(aperture);
    GpuAddressRange first{};
    GpuAddressRange second{};
    check(allocator.allocate(0x1000, 0x1000, first).valid &&
              first.base == 0x100000,
          "first aligned allocation begins at aperture base");
    check(allocator.allocate(0x1000, 0x2000, second).valid &&
              second.base == 0x102000,
          "subsequent allocation honors larger alignment");
    check(!allocator.allocate(0x10000, 0x1000, second).valid,
          "allocation beyond aperture is rejected");
    check(allocator.allocate(64, 3, second).error ==
              ContractError::InvalidAlignment,
          "non-power-of-two alignment is rejected");
}

void testQueueGeometryAndIndices() {
    check(validateQueueGeometry({4096, 16, 16}).valid,
          "valid queue geometry passes");
    check(!validateQueueGeometry({3000, 16, 16}).valid,
          "non-power-of-two ring size is rejected");
    check(!validateQueueGeometry({4096, 128, 16}).valid,
          "oversized queue entry is rejected");

    QueueIndexModel queue(4);
    check(queue.capacity() == 3, "one ring slot remains unused to distinguish full");
    check(queue.push().valid && queue.push().valid && queue.push().valid,
          "queue accepts its usable capacity");
    check(queue.push().error == ContractError::QueueFull,
          "queue refuses producer overrun");
    check(queue.pop().valid && queue.pop().valid && queue.pop().valid,
          "consumer drains queued entries");
    check(queue.pop().error == ContractError::QueueEmpty,
          "queue refuses consumer underrun");
    check(queue.push().valid && queue.pop().valid,
          "queue indices can be reused after draining");
}

}  // namespace

int main() {
    testFirmwareMetadataGate();
    testAddressRangeAndAllocator();
    testQueueGeometryAndIndices();
    if (failures != 0) {
        std::fprintf(stderr, "%d of %d bring-up model checks failed\n",
                     failures, checks);
        return 1;
    }
    std::printf("Navi33 bring-up model checks: %d passed, 0 failed\n", checks);
    return 0;
}
