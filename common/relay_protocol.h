#ifndef TIBERIAN_DAWN_RELAY_PROTOCOL_H
#define TIBERIAN_DAWN_RELAY_PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace TiberianDawnRelay
{
constexpr std::uint8_t ProtocolVersion = 1;
constexpr std::uint32_t CompatibilityVersion = 1;
constexpr std::size_t HeaderSize = 24;
constexpr std::size_t MaximumPayloadSize = 1024;
constexpr std::size_t MaximumFrameSize = HeaderSize + MaximumPayloadSize;

enum class MessageKind : std::uint8_t { Data = 1, Ping = 2, Pong = 3 };

struct Message
{
    MessageKind kind = MessageKind::Data;
    std::uint32_t source_peer = 0;
    std::uint32_t target_peer = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> payload;
};

enum class DecodeResult
{
    Ok,
    TooShort,
    TooLarge,
    BadMagic,
    UnsupportedVersion,
    UnsupportedKind,
    InvalidLength,
    InvalidPeer
};

bool Encode(const Message& message, std::vector<std::uint8_t>& output);
DecodeResult Decode(const std::uint8_t* data, std::size_t size, Message& output);
const char* DecodeResultText(DecodeResult result);
std::uint32_t PeerTargetFromNode(const std::uint8_t node[6], bool marked_broadcast);
} // namespace TiberianDawnRelay

#endif
