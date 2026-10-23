#include "Preparation.hpp"
#include "Fixture.hpp"
#include "DiscoveryFixture.hpp"
#include <type_traits>
using namespace cherryred::navi33;

static_assert(!std::is_copy_constructible_v<Preparation>);
static_assert(!std::is_move_constructible_v<Preparation>);
static_assert(!std::is_copy_constructible_v<VramArena>);

int main() {
    auto bytes = fixture();
    FirmwareInput inputs[] = {
        {FirmwareKind::Pfp, bytes.data(), bytes.size(), {2, 7, 99, 3}},
        {FirmwareKind::Me, bytes.data(), bytes.size(), {2, 7, 99, 3}}
    };
    Preparation prep;
    CHECK(prep.start().error == PreparationError::InvalidState);
    CHECK(prep.prepare({0, 4096}, nullptr, 0, inputs, 2, 64).error == PreparationError::InvalidState);
    CHECK(prep.identify(0x10de, 0x7480).error == PreparationError::WrongDevice);
    CHECK(prep.identify(0x1002, 0x7550).error == PreparationError::WrongDevice);
    CHECK(prep.identify(0x1002, 0x7480).error == PreparationError::None);
    CHECK(prep.identify(0x1002, 0x7480).error == PreparationError::InvalidState);
    CHECK(prep.prepare({0, 4096}, nullptr, 0, nullptr, 1, 64).error == PreparationError::InvalidRequest);
    inputs[1].kind = FirmwareKind::Pfp;
    CHECK(prep.prepare({0, 4096}, nullptr, 0, inputs, 2, 64).error == PreparationError::DuplicateFirmware);
    inputs[1].kind = FirmwareKind::Me;
    inputs[1].expected.ipMajor = 12;
    CHECK(prep.prepare({0, 4096}, nullptr, 0, inputs, 2, 64).error == PreparationError::FirmwareMismatch);
    inputs[1].expected.ipMajor = 99;
    inputs[1].size--;
    CHECK(prep.prepare({0, 4096}, nullptr, 0, inputs, 2, 64).error == PreparationError::FirmwareEnvelope);
    inputs[1].size++;
    CHECK(prep.liveAllocations() == 0 && prep.firmwareCount() == 0);

    // Failure on the second allocation rolls the first back, retaining the
    // identified state so a corrected request can be retried.
    const AddressRange reserved{0, 64};
    const auto result = prep.prepare({0, 256}, &reserved, 1, inputs, 2, 64);
    CHECK(result.error == PreparationError::Memory && result.firmwareIndex == 1);
    CHECK(result.memoryError == MemoryError::OutOfSpace);
    CHECK(prep.state() == PreparationState::Identified);
    CHECK(prep.liveAllocations() == 0 && prep.firmwareCount() == 0 && prep.firmware(0) == nullptr);
    CHECK(prep.prepare({0, 4096}, &reserved, 1, inputs, 2, 64).error == PreparationError::None);
    CHECK(prep.state() == PreparationState::Prepared && prep.liveAllocations() == 2);
    CHECK(prep.firmware(0)->staging.range.base == 64 && prep.firmware(1)->staging.range.base == 192);
    CHECK(prep.firmware(2) == nullptr);
    CHECK(!prep.firmware(0)->hasDiscoveryRecord);
    bytes.fill(0); // input bytes were not retained
    CHECK(prep.firmware(0)->info.ucodeVersion == 0x12345678);
    CHECK(prep.start().error == PreparationError::HardwareBackendUnavailable);
    CHECK(prep.state() == PreparationState::Prepared);
    CHECK(prep.prepare({0, 4096}, nullptr, 0, inputs, 2, 64).error == PreparationError::InvalidState);
    prep.stop();
    prep.stop();
    CHECK(prep.liveAllocations() == 0 && prep.firmwareCount() == 0);
    CHECK(prep.state() == PreparationState::Stopped);
    CHECK(prep.identify(0x1002, 0x7480).error == PreparationError::InvalidState);
    CHECK(prep.start().error == PreparationError::InvalidState);

    // New evidence-aware path preserves the original prepare() contract.
    // Full revision and attributes must agree, not merely major/minor.
    bytes = fixture();
    auto discoveryBytes = discoveryFixture();
    Preparation observed;
    DiscoveryRequirement requirements[] = {{11, 0, 99, 3, 7, 0x42}, {11, 0, 99, 3, 7, 0x42}};
    auto attempt = [&](uint64_t size = 4096) {
        return observed.prepareFromDiscovery(discoveryBytes.data(), discoveryBytes.size(),
            requirements, {0, size}, &reserved, 1, inputs, 2, 64);
    };
    CHECK(attempt().error == PreparationError::InvalidState);
    CHECK(observed.identify(0x1002, 0x7480).error == PreparationError::None);
    discoveryBytes[8] ^= 1;
    auto rejected = attempt();
    CHECK(rejected.error == PreparationError::DiscoveryInvalid);
    CHECK(rejected.discoveryError == DiscoveryError::BinaryChecksum);
    discoveryBytes[8] ^= 1;
    requirements[1].hwId = 255;
    rejected = attempt();
    CHECK(rejected.error == PreparationError::MissingIp && rejected.firmwareIndex == 1);
    requirements[1].hwId = 11;
    requirements[1].revision = 8;
    CHECK(attempt().error == PreparationError::IpMismatch);
    requirements[1].revision = 7;
    requirements[1].attributes = 0;
    CHECK(attempt().error == PreparationError::IpMismatch);
    requirements[1].attributes = 0x42;
    CHECK(observed.firmwareCount() == 0 && observed.liveAllocations() == 0);
    CHECK(attempt(256).error == PreparationError::Memory);
    CHECK(observed.firmwareCount() == 0 && observed.liveAllocations() == 0);
    CHECK(attempt().error == PreparationError::None);
    CHECK(observed.firmware(0)->hasDiscoveryRecord && observed.firmware(1)->hasDiscoveryRecord);
    CHECK(observed.firmware(0)->discovery.bases[0] == 0x100);
    discoveryBytes.fill(0);
    CHECK(observed.firmware(1)->discovery.major == 99);
    CHECK(observed.firmware(1)->discovery.revision == 7);
    CHECK(observed.firmware(1)->discovery.attributes == 0x42);
    CHECK(observed.start().error == PreparationError::HardwareBackendUnavailable);
    observed.stop();
    CHECK(observed.firmwareCount() == 0 && observed.liveAllocations() == 0);
}
