# Multiplayer guide

Multiplayer is available as a cross-platform beta in both Apple apps. It uses
the original Tiberian Dawn deterministic lockstep simulation and classic match
lobby. The Apple setup sheet adds two transports without changing maps, units,
rules, or original game data:

- **Local Network** uses direct UDP discovery and traffic on the current LAN.
- **Private Internet Room** tunnels the same bounded game packets through a
  TLS WebSocket relay at `sportaktivfitness.de`.

Two to six players are supported. iPad and Mac can join one another. Every
player needs the same application version and matching, legally supplied C&C
Gold game data. The relay never receives maps, disc files, saves, or the game
simulation; it only forwards ephemeral opaque packets between members of one
private room.

## Start a local match

1. Put every device on the same local network and open **Multiplayer**.
2. Choose **Local Network**. On iPad, allow the one-time local-network prompt.
3. One player chooses **New Game** in the classic lobby and configures the
   match. The other players select that game when it appears.
4. Choose player names, factions, colors, map and options, then start normally.

If discovery fails, verify that Wi-Fi client isolation is disabled and that the
Mac firewall permits incoming connections for Tiberian Dawn. Guest Wi-Fi often
blocks device-to-device traffic; private Internet rooms work around that.

## Start a private Internet match

1. The host opens **Multiplayer** and chooses **Create Private Internet Room**.
2. The app connects over TLS and copies a complete invitation to the clipboard.
   Share the whole invitation privately with the intended players.
3. The host continues and chooses **New Game** in the classic lobby.
4. Other players choose **Join Internet Invitation**, paste the complete
   invitation, and connect. The host's classic game then appears for joining.

The visible six-character portion is only a convenient room code; the text
after it is the high-entropy room secret. A room accepts at most six peers,
expires after six hours of inactivity, and closes when its host leaves. There
are no accounts, public room lists, analytics, persistent chat logs, or room
history.

## Current beta boundaries

- A suspended iPad cannot advance a lockstep match. Returning quickly may let
  the existing timeout/retry logic recover; a longer suspension ends the game.
- There is no host migration, join-in-progress, multiplayer save/restore,
  spectator mode, ranked matchmaking, or cheat prevention.
- Presentation settings—resolution scaler, artwork mode, language and cursor
  size—may differ per player. Simulation-relevant game data must match.
- The compatibility version is checked before Internet room membership. The
  classic lobby and engine retain their original packet sequencing,
  acknowledgement, retry and deterministic frame controls.

## Relay operation and deployment

The complete relay source is under `multiplayer/relay`. It has no runtime npm
dependencies and runs on Node.js 20 or newer as an unprivileged systemd user.
The production instance listens only on `127.0.0.1:8820`; Apache exposes:

```text
wss://sportaktivfitness.de/tiberian-dawn-relay
https://sportaktivfitness.de/tiberian-dawn-relay/healthz
```

The process caps packet size, peers per room, total connections, send backlog,
message rate, control-message size and idle lifetime. The service stores rooms
only in memory, and its Apache route is excluded from the ordinary access log.
Restarting it intentionally ends all active Internet rooms but does not affect
LAN play. The production systemd and Apache snippets are checked in beside the
server so the deployed boundary is auditable and reproducible.

Run the local and production relay checks with:

```sh
node --test multiplayer/relay/test/*.test.mjs
node multiplayer/relay/test/remote-smoke.mjs
```

The second command creates two temporary clients through the production TLS
endpoint, authenticates a private room and verifies binary broadcast routing.

On macOS, the same path can also be verified through Apple's native WebSocket
implementation—the API used by both apps:

```sh
xcrun swiftc -parse-as-library \
  multiplayer/relay/test/apple-websocket-smoke.swift \
  -o /tmp/apple-websocket-smoke
/tmp/apple-websocket-smoke
```
