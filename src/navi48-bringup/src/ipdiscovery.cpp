//
//  ipdiscovery.cpp
//  RDNA4FB
//
//  See ipdiscovery.hpp for scope and the ROM this was validated against.
//

#include "ipdiscovery.hpp"

// binary_header (discovery.h): u32 signature, u16 version_major,
// u16 version_minor, u16 binary_checksum, u16 binary_size, then the
// table_info list (v1: at +12; v2: u16 num_tables u16 pad, list at +16).
static constexpr uint32_t kBinarySignature = 0x28211407;
// table_info: u16 offset (from binary start), u16 checksum, u16 size, u16 pad.
// Table index 0 is IP_DISCOVERY.
// ip_discovery_header: u32 "IPDS", u16 version, u16 size, u32 id,
// u16 num_dies, then die_info[16] {u16 die_id; u16 die_offset} — offsets
// relative to the *binary* start, not the table.
static constexpr uint32_t kIpdsSignature = 0x53445049; // "IPDS" LE

bool IpDiscovery::readU8(size_t off, uint8_t &v) const {
	if (off >= size) return false;
	v = data[off];
	return true;
}

bool IpDiscovery::readU16(size_t off, uint16_t &v) const {
	// Subtraction-based bounds checks avoid overflow in off + width.
	if (off > size || size - off < 2u) return false;
	v = static_cast<uint16_t>(data[off] | (data[off + 1] << 8));
	return true;
}

bool IpDiscovery::readU32(size_t off, uint32_t &v) const {
	if (off > size || size - off < 4u) return false;
	v = static_cast<uint32_t>(data[off]) |
	    (static_cast<uint32_t>(data[off + 1]) << 8) |
	    (static_cast<uint32_t>(data[off + 2]) << 16) |
	    (static_cast<uint32_t>(data[off + 3]) << 24);
	return true;
}

bool IpDiscovery::validateBinary(size_t offset) {
	// Do not form offset + field-width until the caller buffer is known to
	// contain the fixed binary header. readU* also uses subtraction checks.
	if (offset > size || size - offset < 12u)
		return false;

	uint32_t signature;
	uint16_t vmajor, checksum, binSize;
	if (!readU32(offset, signature) || signature != kBinarySignature ||
	    !readU16(offset + 4, vmajor) || !readU16(offset + 8, checksum) ||
	    !readU16(offset + 10, binSize))
		return false;
	if (static_cast<size_t>(binSize) > size - offset)
		return false;

	const size_t binEnd = offset + static_cast<size_t>(binSize);
	if (binSize < 12u)
		return false;

	// Checksum: 16-bit wrapping byte sum from the checksum field through
	// the declared binary end (format behavior).
	uint16_t sum = 0;
	for (size_t i = offset + 10; i < binEnd; ++i)
		sum = static_cast<uint16_t>(sum + data[i]);
	if (sum != checksum)
		return false;

	// v1 has six fixed table_info entries. v2 has a counted list at +16.
	// Validate the ENTIRE list against binSize before reading any entry.
	size_t tableListRel = 12u;
	size_t tableCount = 6u;
	if (vmajor >= 2u) {
		uint16_t count;
		if (binSize < 16u || !readU16(offset + 12u, count) || count == 0u)
			return false;
		tableListRel = 16u;
		tableCount = static_cast<size_t>(count);
	}
	const size_t tableListBytes = tableCount * 8u;
	if (tableListRel > binSize ||
	    tableListBytes > static_cast<size_t>(binSize) - tableListRel)
		return false;
	const size_t tableListEndRel = tableListRel + tableListBytes;

	// IP_DISCOVERY is table_list[0]; table_info is 8 bytes.
	const size_t tableList = offset + tableListRel;
	uint16_t ipdOffset, ipdSize;
	if (!readU16(tableList, ipdOffset) || !readU16(tableList + 4u, ipdSize) ||
	    ipdOffset == 0u || ipdSize < 80u)
		return false;

	// Offsets are relative to the binary start. Keep the table within both
	// the declared binary and caller buffer, and reject header/list overlap.
	if (static_cast<size_t>(ipdOffset) < tableListEndRel ||
	    static_cast<size_t>(ipdOffset) > binSize ||
	    static_cast<size_t>(ipdSize) > static_cast<size_t>(binSize) - ipdOffset)
		return false;
	const size_t ipd = offset + static_cast<size_t>(ipdOffset);
	const size_t ipdEnd = ipd + static_cast<size_t>(ipdSize);

	uint32_t sig;
	uint16_t ipdVersion, declaredIpdSize, numDies;
	if (!readU32(ipd, sig) || sig != kIpdsSignature ||
	    !readU16(ipd + 4u, ipdVersion) ||
	    !readU16(ipd + 6u, declaredIpdSize) ||
	    declaredIpdSize != ipdSize || declaredIpdSize < 80u ||
	    !readU16(ipd + 12u, numDies) || numDies == 0u)
		return false;
	// Version is recorded for evidence; the parser does not infer Navi33 IP
	// versions or enable hardware paths from an untrusted table.
	(void)ipdVersion;

	// The fixed IPDS header (including 16 die_info entries) is 80 bytes.
	// die_info[0] is at +14; its offset member is at +16.
	uint16_t dieOffset;
	if (!readU16(ipd + 16u, dieOffset) || dieOffset == 0u)
		return false;
	if (static_cast<size_t>(dieOffset) < static_cast<size_t>(ipdOffset) + 80u ||
	    static_cast<size_t>(dieOffset) > binSize ||
	    4u > static_cast<size_t>(binSize) - dieOffset)
		return false;
	const size_t die = offset + static_cast<size_t>(dieOffset);
	// A die header must also fit inside this IP_DISCOVERY table, not just
	// elsewhere in the binary or caller buffer.
	if (die < ipd || die > ipdEnd || ipdEnd - die < 4u)
		return false;

	uint16_t nips;
	if (!readU16(die + 2u, nips) || nips == 0u || nips > 256u)
		return false;

	binBase = offset;
	tableEnd = ipdEnd;
	dieBase = die;
	numIps = nips;
	return true;
}

bool IpDiscovery::init(const uint8_t *buffer, size_t bufferSize) {
	data = buffer;
	size = bufferSize;
	valid = false;
	binBase = 0;
	tableEnd = 0;
	dieBase = 0;
	numIps = 0;
	if (!data || size < 512)
		return false;

	// The binary is not alignment-guaranteed within a flash dump (this ROM:
	// 0x34900); scan for the signature on 4-byte steps.
	for (size_t off = 0; off <= size - 512u; off += 4u) {
		uint32_t sig;
		if (readU32(off, sig) && sig == kBinarySignature && validateBinary(off)) {
			valid = true;
			return true;
		}
	}
	return false;
}

bool IpDiscovery::ipAt(uint16_t index, IpEntry &out) const {
	if (!valid || index >= numIps)
		return false;

	// ip_v3/v4 with 32-bit bases: u16 hw_id, u8 instance, u8 num_base,
	// u8 major, u8 minor, u8 revision, u8 sub/variant, u32 bases[].
	// Walk the variable-size records to the requested index. The walk is
	// bounded by the declared IP_DISCOVERY table size, not by the caller's
	// buffer: the buffer can be much larger than the table it declares, and a
	// truncated table must not let a record run into the bytes after it.
	size_t p = dieBase + 4;
	for (uint16_t i = 0; ; i++) {
		if (p > tableEnd || tableEnd - p < 8u)
			return false;
		uint16_t hwId;
		uint8_t inst, nbase, major, minor, rev;
		if (!readU16(p, hwId) || !readU8(p + 2, inst) || !readU8(p + 3, nbase) ||
		    !readU8(p + 4, major) || !readU8(p + 5, minor) || !readU8(p + 6, rev))
			return false;
		if (tableEnd - p < 8u + 4u * static_cast<size_t>(nbase))
			return false;

		if (i == index) {
			out.hwId = hwId;
			out.instance = inst;
			out.major = major;
			out.minor = minor;
			out.revision = rev;
			out.numBases = nbase < MaxBases ? nbase : MaxBases;
			for (uint8_t b = 0; b < out.numBases; b++)
				if (!readU32(p + 8 + 4u * b, out.bases[b]))
					return false;
			return true;
		}
		p += 8 + 4u * nbase;
	}
}

bool IpDiscovery::findIp(uint16_t hwId, uint8_t instance, IpEntry &out) const {
	for (uint16_t i = 0; i < numIps; i++) {
		if (!ipAt(i, out))
			return false;
		if (out.hwId == hwId && out.instance == instance)
			return true;
	}
	return false;
}

bool IpDiscovery::regByteOffset(uint16_t hwId, uint8_t instance, uint8_t segment,
                                uint32_t regDwordOffset, uint32_t &byteOffset) const {
	IpEntry ip;
	if (!findIp(hwId, instance, ip) || segment >= ip.numBases)
		return false;
	// Reject overflow *before* producing an offset. A wrapped result would be
	// a different but valid-looking MMIO address, and on refusal byteOffset
	// must keep the caller's value.
	if (ip.bases[segment] > UINT32_MAX - regDwordOffset)
		return false;
	uint32_t dwords = ip.bases[segment] + regDwordOffset;
	if (dwords > UINT32_MAX / 4u)
		return false;
	byteOffset = dwords * 4u;
	return true;
}
