import assert from 'node:assert/strict';

const url = process.env.TD_RELAY_URL ?? 'wss://sportaktivfitness.de/tiberian-dawn-relay';

function openSocket() {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(url);
    socket.binaryType = 'arraybuffer';
    const timer = setTimeout(() => reject(new Error('open timeout')), 10_000);
    socket.addEventListener('open', () => { clearTimeout(timer); resolve(socket); }, { once: true });
    socket.addEventListener('error', () => { clearTimeout(timer); reject(new Error('WebSocket connection failed')); }, { once: true });
  });
}

function nextMessage(socket) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('message timeout')), 10_000);
    socket.addEventListener('message', (event) => { clearTimeout(timer); resolve(event.data); }, { once: true });
  });
}

function packet(target, content) {
  const payload = Buffer.from(content);
  const frame = Buffer.alloc(24 + payload.length);
  frame.write('TDR1');
  frame[4] = 1;
  frame[5] = 1;
  frame.writeUInt32BE(target, 12);
  frame.writeUInt32BE(1, 16);
  frame.writeUInt32BE(payload.length, 20);
  payload.copy(frame, 24);
  return frame;
}

const host = await openSocket();
const hostReply = nextMessage(host);
host.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 1 }));
const created = JSON.parse(await hostReply);
assert.equal(created.type, 'ready');

const guest = await openSocket();
const guestReply = nextMessage(guest);
guest.send(JSON.stringify({ type: 'join', protocol: 1, compatibility: 1, invite: created.invite }));
const joined = JSON.parse(await guestReply);
assert.equal(joined.type, 'ready');

const relayedReply = nextMessage(guest);
host.send(packet(0, 'production-smoke'));
const relayed = Buffer.from(await relayedReply);
assert.equal(relayed.readUInt32BE(8), created.peerId);
assert.equal(relayed.subarray(24).toString(), 'production-smoke');

host.close();
guest.close();
console.log(`Relay smoke passed: ${url}`);
