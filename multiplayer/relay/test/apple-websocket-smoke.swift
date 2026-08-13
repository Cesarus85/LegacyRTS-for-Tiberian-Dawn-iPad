import Foundation

private enum SmokeError: Error, CustomStringConvertible {
    case message(String)

    var description: String {
        switch self {
        case .message(let text): return text
        }
    }
}

private final class RelayClient {
    private let session: URLSession
    let task: URLSessionWebSocketTask

    init(url: URL) {
        let configuration = URLSessionConfiguration.ephemeral
        configuration.timeoutIntervalForRequest = 15
        configuration.timeoutIntervalForResource = 30
        session = URLSession(configuration: configuration)
        task = session.webSocketTask(with: url)
        task.resume()
    }

    deinit {
        task.cancel(with: .goingAway, reason: nil)
        session.invalidateAndCancel()
    }

    func control(_ object: [String: Any]) async throws -> [String: Any] {
        let data = try JSONSerialization.data(withJSONObject: object)
        guard let text = String(data: data, encoding: .utf8) else {
            throw SmokeError.message("Could not encode control message")
        }
        try await task.send(.string(text))
        for _ in 0..<4 {
            let message = try await task.receive()
            guard case .string(let responseText) = message,
                  let responseData = responseText.data(using: .utf8),
                  let response = try JSONSerialization.jsonObject(with: responseData) as? [String: Any] else {
                continue
            }
            if response["type"] as? String == "error" {
                throw SmokeError.message(response["message"] as? String ?? "Relay rejected request")
            }
            if response["type"] as? String == "ready" { return response }
        }
        throw SmokeError.message("Relay did not return a ready message")
    }

    func receiveData() async throws -> Data {
        for _ in 0..<4 {
            if case .data(let data) = try await task.receive() { return data }
        }
        throw SmokeError.message("Relay did not return binary data")
    }
}

private func appendUInt32(_ value: UInt32, to data: inout Data) {
    data.append(UInt8(truncatingIfNeeded: value >> 24))
    data.append(UInt8(truncatingIfNeeded: value >> 16))
    data.append(UInt8(truncatingIfNeeded: value >> 8))
    data.append(UInt8(truncatingIfNeeded: value))
}

private func readUInt32(_ data: Data, at offset: Int) -> UInt32 {
    (UInt32(data[offset]) << 24)
        | (UInt32(data[offset + 1]) << 16)
        | (UInt32(data[offset + 2]) << 8)
        | UInt32(data[offset + 3])
}

private func relayFrame(source: UInt32, target: UInt32, payload: Data) -> Data {
    var frame = Data("TDR1".utf8)
    frame.append(1) // protocol
    frame.append(1) // data
    frame.append(contentsOf: [0, 0])
    appendUInt32(source, to: &frame)
    appendUInt32(target, to: &frame)
    appendUInt32(77, to: &frame)
    appendUInt32(UInt32(payload.count), to: &frame)
    frame.append(payload)
    return frame
}

@main
private struct AppleWebSocketSmoke {
    static func main() async {
        do {
            let endpoint = CommandLine.arguments.dropFirst().first
                ?? "wss://sportaktivfitness.de/tiberian-dawn-relay"
            guard let url = URL(string: endpoint) else {
                throw SmokeError.message("Invalid relay URL")
            }

            let host = RelayClient(url: url)
            let hostReady = try await host.control([
                "type": "create", "protocol": 1, "compatibility": 1
            ])
            guard let invitation = hostReady["invite"] as? String,
                  let hostPeer = (hostReady["peerId"] as? NSNumber)?.uint32Value else {
                throw SmokeError.message("Host ready message is incomplete")
            }

            let guest = RelayClient(url: url)
            let guestReady = try await guest.control([
                "type": "join", "protocol": 1, "compatibility": 1, "invite": invitation
            ])
            guard let guestPeer = (guestReady["peerId"] as? NSNumber)?.uint32Value else {
                throw SmokeError.message("Guest ready message is incomplete")
            }

            let payload = Data([0x43, 0x26, 0x43, 0x01])
            try await host.task.send(.data(relayFrame(source: hostPeer, target: guestPeer, payload: payload)))
            let received = try await guest.receiveData()
            guard received.count == 24 + payload.count,
                  received.prefix(4) == Data("TDR1".utf8),
                  readUInt32(received, at: 8) == hostPeer,
                  readUInt32(received, at: 12) == guestPeer,
                  received.suffix(payload.count) == payload else {
                throw SmokeError.message("Relayed frame did not match")
            }
            print("Apple WebSocket smoke passed: \(endpoint)")
        } catch {
            fputs("Apple WebSocket smoke failed: \(error)\n", stderr)
            exit(1)
        }
    }
}
