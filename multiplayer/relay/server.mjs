import crypto from 'node:crypto';
import http from 'node:http';

export const PROTOCOL_VERSION = 1;
export const COMPATIBILITY_VERSION = 1;
export const MAX_PEERS = 6;
export const MAX_CONNECTIONS = 512;
export const MAX_FRAME = 1048;
const MAX_CONTROL = 4096;
const ROOM_TTL_MS = 6 * 60 * 60 * 1000;
const IDLE_SOCKET_MS = 90 * 1000;
const CODE_ALPHABET = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789';
const MAGIC = Buffer.from('TDR1');

function roomCode(rooms) {
  for (let attempt = 0; attempt < 100; attempt++) {
    let code = '';
    for (let index = 0; index < 6; index++) {
      code += CODE_ALPHABET[crypto.randomInt(CODE_ALPHABET.length)];
    }
    if (!rooms.has(code)) return code;
  }
  throw new Error('room code space exhausted');
}

function peerId(room) {
  for (;;) {
    const id = crypto.randomBytes(4).readUInt32BE(0);
    if (id !== 0 && id !== 0xffffffff && !room.peers.has(id)) return id;
  }
}

function constantTimeTextEqual(left, right) {
  const a = Buffer.from(String(left));
  const b = Buffer.from(String(right));
  return a.length === b.length && crypto.timingSafeEqual(a, b);
}

class WebSocketConnection {
  constructor(socket, onMessage, onClose) {
    this.socket = socket;
    this.onMessage = onMessage;
    this.onClose = onClose;
    this.buffer = Buffer.alloc(0);
    this.closed = false;
    this.lastActivity = Date.now();
    this.tokens = 200;
    this.lastRefill = Date.now();
    this.framesReceived = 0;
    this.framesForwarded = 0;
    this.broadcastsReceived = 0;
    this.unicastsReceived = 0;
    socket.on('data', (chunk) => this.feed(chunk));
    socket.on('close', () => this.finish());
    socket.on('error', () => this.finish());
  }

  allowed() {
    const now = Date.now();
    this.tokens = Math.min(200, this.tokens + (now - this.lastRefill) * 0.1);
    this.lastRefill = now;
    if (this.tokens < 1) return false;
    this.tokens--;
    return true;
  }

  feed(chunk) {
    if (this.closed) return;
    this.lastActivity = Date.now();
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (this.buffer.length >= 2) {
      const first = this.buffer[0];
      const second = this.buffer[1];
      const fin = (first & 0x80) !== 0;
      const opcode = first & 0x0f;
      const masked = (second & 0x80) !== 0;
      let length = second & 0x7f;
      let offset = 2;
      if (!fin || !masked || (first & 0x70) !== 0) return this.close(1002, 'invalid frame');
      if (length === 126) {
        if (this.buffer.length < 4) return;
        length = this.buffer.readUInt16BE(2);
        offset = 4;
      } else if (length === 127) {
        return this.close(1009, 'frame too large');
      }
      const limit = opcode === 1 ? MAX_CONTROL : MAX_FRAME;
      if (length > limit || (opcode >= 8 && length > 125)) return this.close(1009, 'frame too large');
      if (this.buffer.length < offset + 4 + length) return;
      const mask = this.buffer.subarray(offset, offset + 4);
      offset += 4;
      const payload = Buffer.from(this.buffer.subarray(offset, offset + length));
      for (let index = 0; index < payload.length; index++) payload[index] ^= mask[index & 3];
      this.buffer = this.buffer.subarray(offset + length);
      if (!this.allowed()) return this.close(1008, 'rate limit');
      if (opcode === 8) return this.close(1000, 'bye');
      if (opcode === 9) { this.send(10, payload); continue; }
      if (opcode === 10) continue;
      if (opcode !== 1 && opcode !== 2) return this.close(1003, 'unsupported frame');
      this.onMessage(this, opcode, payload);
      if (this.closed) return;
    }
  }

  send(opcode, payload) {
    if (this.closed || this.socket.destroyed) return;
    if (this.socket.writableLength > 1024 * 1024) return this.close(1008, 'slow receiver');
    const body = Buffer.isBuffer(payload) ? payload : Buffer.from(payload);
    let header;
    if (body.length < 126) {
      header = Buffer.from([0x80 | opcode, body.length]);
    } else {
      header = Buffer.alloc(4);
      header[0] = 0x80 | opcode;
      header[1] = 126;
      header.writeUInt16BE(body.length, 2);
    }
    this.socket.write(Buffer.concat([header, body]));
  }

  json(value) { this.send(1, JSON.stringify(value)); }

  close(code, reason) {
    if (this.closed) return;
    const text = Buffer.from(String(reason).slice(0, 100));
    const body = Buffer.alloc(2 + text.length);
    body.writeUInt16BE(code, 0);
    text.copy(body, 2);
    this.send(8, body);
    this.socket.end();
    this.finish();
  }

  finish() {
    if (this.closed) return;
    this.closed = true;
    this.onClose(this);
  }
}

export function createRelayServer(options = {}) {
  const rooms = new Map();
  const connections = new Set();
  const host = options.host ?? process.env.TD_RELAY_HOST ?? '127.0.0.1';
  const port = Number(options.port ?? process.env.TD_RELAY_PORT ?? 8820);

  function leave(connection) {
    connections.delete(connection);
    const room = connection.room;
    if (!room || !connection.peerId) return;
    room.peers.delete(connection.peerId);
    if (room.hostId === connection.peerId) {
      const remaining = [...room.peers.values()];
      rooms.delete(room.code);
      for (const peer of remaining) peer.close(1001, 'host left room');
      return;
    }
    room.expiresAt = Date.now() + ROOM_TTL_MS;
    if (room.peers.size === 0) rooms.delete(room.code);
  }

  function fail(connection, code, message) {
    connection.json({ type: 'error', code, message });
  }

  function ready(connection, room, id) {
    connection.room = room;
    connection.peerId = id;
    room.peers.set(id, connection);
    room.expiresAt = Date.now() + ROOM_TTL_MS;
    connection.json({
      type: 'ready',
      protocol: PROTOCOL_VERSION,
      roomCode: room.code,
      invite: `${room.code}-${room.secret}`,
      peerId: id,
      peers: [...room.peers.keys()].filter((peer) => peer !== id)
    });
  }

  function control(connection, payload) {
    if (connection.room) return fail(connection, 'already_joined', 'Connection already belongs to a room.');
    let request;
    try { request = JSON.parse(payload.toString('utf8')); }
    catch { return fail(connection, 'bad_json', 'Invalid control message.'); }
    if (request?.protocol !== PROTOCOL_VERSION) return fail(connection, 'version', 'Protocol version mismatch.');
    if (request?.compatibility !== COMPATIBILITY_VERSION) return fail(connection, 'compatibility', 'Game versions are not multiplayer-compatible.');
    if (request.type === 'create') {
      const code = roomCode(rooms);
      const room = {
        code,
        secret: crypto.randomBytes(18).toString('base64url'),
        peers: new Map(),
        hostId: 0,
        compatibility: request.compatibility,
        expiresAt: Date.now() + ROOM_TTL_MS
      };
      rooms.set(code, room);
      room.hostId = peerId(room);
      return ready(connection, room, room.hostId);
    }
    if (request.type !== 'join' || typeof request.invite !== 'string') {
      return fail(connection, 'bad_request', 'Expected create or join.');
    }
    request.invite = request.invite.trim();
    const separator = request.invite.indexOf('-');
    if (separator !== 6) return fail(connection, 'invite', 'Invalid invitation.');
    const code = request.invite.slice(0, 6).toUpperCase();
    const secret = request.invite.slice(7);
    const room = rooms.get(code);
    if (!room || room.expiresAt < Date.now() || !constantTimeTextEqual(secret, room.secret)) {
      return fail(connection, 'invite', 'Invitation is invalid or expired.');
    }
    if (room.compatibility !== request.compatibility) return fail(connection, 'compatibility', 'Game versions are not multiplayer-compatible.');
    if (room.peers.size >= MAX_PEERS) return fail(connection, 'full', 'Room is full.');
    ready(connection, room, peerId(room));
  }

  function relay(connection, payload) {
    if (!connection.room) return fail(connection, 'not_joined', 'Join a room first.');
    if (payload.length < 24 || payload.length > MAX_FRAME
        || !payload.subarray(0, 4).equals(MAGIC)
        || payload[4] !== PROTOCOL_VERSION
        || payload[5] < 1 || payload[5] > 3
        || payload.readUInt32BE(20) !== payload.length - 24) {
      return connection.close(1008, 'invalid relay packet');
    }
    const claimedSource = payload.readUInt32BE(8);
    const target = payload.readUInt32BE(12);
    if ((claimedSource !== 0 && claimedSource !== connection.peerId) || target === connection.peerId) {
      return connection.close(1008, 'invalid routing');
    }
    const forwarded = Buffer.from(payload);
    forwarded.writeUInt32BE(connection.peerId, 8);
    connection.framesReceived++;
    if (target === 0) connection.broadcastsReceived++;
    else connection.unicastsReceived++;
    connection.room.expiresAt = Date.now() + ROOM_TTL_MS;
    if (target === 0) {
      for (const [id, peer] of connection.room.peers) {
        if (id !== connection.peerId) {
          peer.send(2, forwarded);
          peer.framesForwarded++;
        }
      }
    } else {
      const peer = connection.room.peers.get(target);
      if (peer) {
        peer.send(2, forwarded);
        peer.framesForwarded++;
      }
    }
  }

  const server = http.createServer((request, response) => {
    if (request.url === '/healthz') {
      response.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      response.end(JSON.stringify({ ok: true, protocol: PROTOCOL_VERSION, rooms: rooms.size, peers: connections.size }));
      return;
    }
    // Loopback-only operational diagnostics. Apache intentionally exposes
    // only /healthz and the WebSocket endpoint, never this route. Counters
    // contain no payloads, invitations, IP addresses, or player names.
    if (request.url === '/debugz') {
      response.writeHead(200, { 'content-type': 'application/json', 'cache-control': 'no-store' });
      response.end(JSON.stringify({
        rooms: [...rooms.values()].map((room) => ({
          peers: [...room.peers.entries()].map(([id, peer]) => ({
            host: id === room.hostId,
            framesReceived: peer.framesReceived,
            framesForwarded: peer.framesForwarded,
            broadcastsReceived: peer.broadcastsReceived,
            unicastsReceived: peer.unicastsReceived
          }))
        }))
      }));
      return;
    }
    response.writeHead(404).end();
  });

  server.on('upgrade', (request, socket) => {
    if (request.url !== '/tiberian-dawn-relay'
        || request.headers.upgrade?.toLowerCase() !== 'websocket'
        || request.headers['sec-websocket-version'] !== '13') {
      socket.end('HTTP/1.1 404 Not Found\r\n\r\n');
      return;
    }
    if (connections.size >= MAX_CONNECTIONS) {
      socket.end('HTTP/1.1 503 Service Unavailable\r\nRetry-After: 30\r\n\r\n');
      return;
    }
    const key = request.headers['sec-websocket-key'];
    if (typeof key !== 'string' || !/^[A-Za-z0-9+/]{22}==$/.test(key)) {
      socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
      return;
    }
    const accept = crypto.createHash('sha1').update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`).digest('base64');
    socket.write('HTTP/1.1 101 Switching Protocols\r\n'
      + 'Upgrade: websocket\r\nConnection: Upgrade\r\n'
      + `Sec-WebSocket-Accept: ${accept}\r\n\r\n`);
    const connection = new WebSocketConnection(socket,
      (client, opcode, payload) => opcode === 1 ? control(client, payload) : relay(client, payload),
      leave);
    connections.add(connection);
  });

  const housekeeping = setInterval(() => {
    const now = Date.now();
    for (const [code, room] of rooms) {
      if (room.expiresAt < now) {
        for (const peer of room.peers.values()) peer.close(1001, 'room expired');
        rooms.delete(code);
      }
    }
    for (const connection of connections) {
      if (now - connection.lastActivity > IDLE_SOCKET_MS) connection.close(1001, 'idle timeout');
      else connection.send(9, Buffer.alloc(0));
    }
  }, 30_000);
  housekeeping.unref();

  return {
    rooms,
    server,
    async listen() {
      await new Promise((resolve, reject) => {
        server.once('error', reject);
        server.listen(port, host, resolve);
      });
      return server.address();
    },
    async close() {
      clearInterval(housekeeping);
      for (const connection of connections) {
        connection.close(1001, 'server shutdown');
        connection.socket.destroy();
      }
      await new Promise((resolve) => server.close(resolve));
    }
  };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const relay = createRelayServer();
  relay.listen().then((address) => {
    console.log(`Tiberian Dawn relay listening on ${address.address}:${address.port}`);
  });
  const shutdown = async () => { await relay.close(); process.exit(0); };
  process.on('SIGTERM', shutdown);
  process.on('SIGINT', shutdown);
}
