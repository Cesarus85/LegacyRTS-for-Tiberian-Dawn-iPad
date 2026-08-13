#include "relay_protocol.h"

#include <algorithm>

namespace TiberianDawnRelay
{
namespace
{
constexpr std::uint8_t Magic[4] = {'T', 'D', 'R', '1'};

void Write32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = static_cast<std::uint8_t>(value >> 24);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

std::uint32_t Read32(const std::uint8_t* bytes)
{
    return (static_cast<std::uint32_t>(bytes[0]) << 24)
        | (static_cast<std::uint32_t>(bytes[1]) << 16)
        | (static_cast<std::uint32_t>(bytes[2]) << 8)
        | static_cast<std::uint32_t>(bytes[3]);
}
}

bool Encode(const Message& message, std::vector<std::uint8_t>& output)
{
    if (message.payload.size() > MaximumPayloadSize
        || (message.source_peer == message.target_peer && message.source_peer != 0)) {
        return false;
    }
    const std::uint8_t kind = static_cast<std::uint8_t>(message.kind);
    if (kind < static_cast<std::uint8_t>(MessageKind::Data)
        || kind > static_cast<std::uint8_t>(MessageKind::Pong)) {
        return false;
    }

    output.assign(HeaderSize + message.payload.size(), 0);
    std::copy(Magic, Magic + sizeof(Magic), output.begin());
    output[4] = ProtocolVersion;
    output[5] = kind;
    Write32(output, 8, message.source_peer);
    Write32(output, 12, message.target_peer);
    Write32(output, 16, message.sequence);
    Write32(output, 20, static_cast<std::uint32_t>(message.payload.size()));
    std::copy(message.payload.begin(), message.payload.end(), output.begin() + HeaderSize);
    return true;
}

DecodeResult Decode(const std::uint8_t* data, std::size_t size, Message& output)
{
    if (data == nullptr || size < HeaderSize) return DecodeResult::TooShort;
    if (size > MaximumFrameSize) return DecodeResult::TooLarge;
    if (!std::equal(Magic, Magic + sizeof(Magic), data)) return DecodeResult::BadMagic;
    if (data[4] != ProtocolVersion) return DecodeResult::UnsupportedVersion;
    if (data[5] < static_cast<std::uint8_t>(MessageKind::Data)
        || data[5] > static_cast<std::uint8_t>(MessageKind::Pong)) {
        return DecodeResult::UnsupportedKind;
    }
    const std::uint32_t payload_size = Read32(data + 20);
    if (payload_size > MaximumPayloadSize || HeaderSize + payload_size != size) return DecodeResult::InvalidLength;
    const std::uint32_t source = Read32(data + 8);
    const std::uint32_t target = Read32(data + 12);
    if (source != 0 && source == target) return DecodeResult::InvalidPeer;

    output.kind = static_cast<MessageKind>(data[5]);
    output.source_peer = source;
    output.target_peer = target;
    output.sequence = Read32(data + 16);
    output.payload.assign(data + HeaderSize, data + size);
    return DecodeResult::Ok;
}

const char* DecodeResultText(DecodeResult result)
{
    switch (result) {
    case DecodeResult::Ok: return "ok";
    case DecodeResult::TooShort: return "frame too short";
    case DecodeResult::TooLarge: return "frame too large";
    case DecodeResult::BadMagic: return "bad magic";
    case DecodeResult::UnsupportedVersion: return "unsupported protocol version";
    case DecodeResult::UnsupportedKind: return "unsupported message kind";
    case DecodeResult::InvalidLength: return "invalid payload length";
    case DecodeResult::InvalidPeer: return "invalid peer routing";
    }
    return "unknown decode error";
}

std::uint32_t PeerTargetFromNode(const std::uint8_t node[6], bool marked_broadcast)
{
    if (marked_broadcast || node == nullptr) return 0;
    bool classic_broadcast = true;
    for (std::size_t index = 0; index < 6; ++index) {
        if (node[index] != 0xff) {
            classic_broadcast = false;
            break;
        }
    }
    if (classic_broadcast) return 0;
    return (static_cast<std::uint32_t>(node[0]) << 24)
        | (static_cast<std::uint32_t>(node[1]) << 16)
        | (static_cast<std::uint32_t>(node[2]) << 8)
        | static_cast<std::uint32_t>(node[3]);
}
} // namespace TiberianDawnRelay
