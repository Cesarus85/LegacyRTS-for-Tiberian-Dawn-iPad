#include "common/relay_protocol.h"

#include <cassert>
#include <cstdint>
#include <vector>

int main()
{
    using namespace TiberianDawnRelay;
    Message original;
    original.source_peer = 0x01020304;
    original.target_peer = 0x05060708;
    original.sequence = 42;
    original.payload = {0, 1, 2, 0xfe, 0xff};

    std::vector<std::uint8_t> wire;
    const bool encoded = Encode(original, wire);
    assert(encoded);
    assert(wire.size() == HeaderSize + original.payload.size());

    Message decoded;
    const DecodeResult decoded_result = Decode(wire.data(), wire.size(), decoded);
    assert(decoded_result == DecodeResult::Ok);
    assert(decoded.source_peer == original.source_peer);
    assert(decoded.target_peer == original.target_peer);
    assert(decoded.sequence == original.sequence);
    assert(decoded.payload == original.payload);

    for (std::size_t size = 0; size < HeaderSize; ++size) {
        const DecodeResult short_result = Decode(wire.data(), size, decoded);
        assert(short_result == DecodeResult::TooShort);
    }
    wire[0] ^= 1;
    const DecodeResult bad_magic = Decode(wire.data(), wire.size(), decoded);
    assert(bad_magic == DecodeResult::BadMagic);
    wire[0] ^= 1;
    wire[4] = 99;
    const DecodeResult bad_version = Decode(wire.data(), wire.size(), decoded);
    assert(bad_version == DecodeResult::UnsupportedVersion);
    wire[4] = ProtocolVersion;
    wire[23]++;
    const DecodeResult bad_length = Decode(wire.data(), wire.size(), decoded);
    assert(bad_length == DecodeResult::InvalidLength);

    Message oversized;
    oversized.payload.resize(MaximumPayloadSize + 1);
    const bool oversized_encoded = Encode(oversized, wire);
    assert(!oversized_encoded);

    const std::uint8_t directed_node[6] = {0x05, 0x06, 0x07, 0x08, 0, 0};
    assert(PeerTargetFromNode(directed_node, false) == 0x05060708);
    assert(PeerTargetFromNode(directed_node, true) == 0);
    const std::uint8_t classic_broadcast_node[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    assert(PeerTargetFromNode(classic_broadcast_node, false) == 0);
    assert(PeerTargetFromNode(nullptr, false) == 0);
    return 0;
}
