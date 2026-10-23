#include "Preparation.hpp"
#include "Fixture.hpp"
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
}
