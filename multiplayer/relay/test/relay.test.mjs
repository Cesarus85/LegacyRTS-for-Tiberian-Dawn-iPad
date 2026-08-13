import assert from 'node:assert/strict';
import test from 'node:test';
import { createRelayServer } from '../server.mjs';

function relayPacket(target, sequence, payload) {
  const body = Buffer.from(payload);
  const packet = Buffer.alloc(24 + body.length);
  packet.write('TDR1', 0, 'ascii');
  packet[4] = 1;
  packet[5] = 1;
  packet.writeUInt32BE(target, 12);
  packet.writeUInt32BE(sequence, 16);
  packet.writeUInt32BE(body.length, 20);
  body.copy(packet, 24);
  return packet;
}

function nextMessage(socket) {
  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(new Error('message timeout')), 2000);
    socket.addEventListener('message', (event) => {
      clearTimeout(timeout);
      resolve(event.data);
    }, { once: true });
  });
}

test('private rooms create, authenticate, broadcast, and unicast', async () => {
  const relay = createRelayServer({ host: '127.0.0.1', port: 0 });
  const address = await relay.listen();
  const url = `ws://127.0.0.1:${address.port}/tiberian-dawn-relay`;
  const host = new WebSocket(url);
  host.binaryType = 'arraybuffer';
  await new Promise((resolve) => host.addEventListener('open', resolve, { once: true }));
  host.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 1 }));
  const hostReady = JSON.parse(await nextMessage(host));
  assert.equal(hostReady.type, 'ready');
  assert.match(hostReady.invite, /^[A-Z2-9]{6}-[A-Za-z0-9_-]{24}$/);

  const guest = new WebSocket(url);
  guest.binaryType = 'arraybuffer';
  await new Promise((resolve) => guest.addEventListener('open', resolve, { once: true }));
  guest.send(JSON.stringify({ type: 'join', protocol: 1, compatibility: 1, invite: `  ${hostReady.invite}\n` }));
  const guestReady = JSON.parse(await nextMessage(guest));
  assert.equal(guestReady.type, 'ready');
  assert.notEqual(guestReady.peerId, hostReady.peerId);

  const broadcastPromise = nextMessage(guest);
  host.send(relayPacket(0, 7, Buffer.from([1, 2, 3])));
  const broadcast = Buffer.from(await broadcastPromise);
  assert.equal(broadcast.readUInt32BE(8), hostReady.peerId);
  assert.deepEqual([...broadcast.subarray(24)], [1, 2, 3]);

  const diagnostics = await (await fetch(`http://127.0.0.1:${address.port}/debugz`)).json();
  assert.equal(diagnostics.rooms.length, 1);
  assert.deepEqual(diagnostics.rooms[0].peers.map((peer) => ({
    host: peer.host,
    framesReceived: peer.framesReceived,
    framesForwarded: peer.framesForwarded,
    broadcastsReceived: peer.broadcastsReceived,
    unicastsReceived: peer.unicastsReceived
  })), [
    { host: true, framesReceived: 1, framesForwarded: 0, broadcastsReceived: 1, unicastsReceived: 0 },
    { host: false, framesReceived: 0, framesForwarded: 1, broadcastsReceived: 0, unicastsReceived: 0 }
  ]);

  const unicastPromise = nextMessage(host);
  guest.send(relayPacket(hostReady.peerId, 8, Buffer.from([4, 5])));
  const unicast = Buffer.from(await unicastPromise);
  assert.equal(unicast.readUInt32BE(8), guestReady.peerId);
  assert.deepEqual([...unicast.subarray(24)], [4, 5]);

  host.close();
  guest.close();
  await relay.close();
});

test('wrong room secret is rejected without joining', async () => {
  const relay = createRelayServer({ host: '127.0.0.1', port: 0 });
  const address = await relay.listen();
  const socket = new WebSocket(`ws://127.0.0.1:${address.port}/tiberian-dawn-relay`);
  await new Promise((resolve) => socket.addEventListener('open', resolve, { once: true }));
  socket.send(JSON.stringify({ type: 'join', protocol: 1, compatibility: 1, invite: 'ABCDEF-wrong' }));
  const response = JSON.parse(await nextMessage(socket));
  assert.equal(response.type, 'error');
  assert.equal(response.code, 'invite');
  socket.close();
  await relay.close();
});

test('incompatible clients are rejected before room membership', async () => {
  const relay = createRelayServer({ host: '127.0.0.1', port: 0 });
  const address = await relay.listen();
  const socket = new WebSocket(`ws://127.0.0.1:${address.port}/tiberian-dawn-relay`);
  await new Promise((resolve) => socket.addEventListener('open', resolve, { once: true }));
  socket.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 999 }));
  const response = JSON.parse(await nextMessage(socket));
  assert.equal(response.type, 'error');
  assert.equal(response.code, 'compatibility');
  socket.close();
  await relay.close();
});

test('a room admits six peers and rejects the seventh', async () => {
  const relay = createRelayServer({ host: '127.0.0.1', port: 0 });
  const address = await relay.listen();
  const url = `ws://127.0.0.1:${address.port}/tiberian-dawn-relay`;
  const sockets = [];
  const host = new WebSocket(url);
  sockets.push(host);
  await new Promise((resolve) => host.addEventListener('open', resolve, { once: true }));
  host.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 1 }));
  const ready = JSON.parse(await nextMessage(host));

  for (let index = 0; index < 5; index++) {
    const guest = new WebSocket(url);
    sockets.push(guest);
    await new Promise((resolve) => guest.addEventListener('open', resolve, { once: true }));
    guest.send(JSON.stringify({ type: 'join', protocol: 1, compatibility: 1, invite: ready.invite }));
    assert.equal(JSON.parse(await nextMessage(guest)).type, 'ready');
  }
  const extra = new WebSocket(url);
  sockets.push(extra);
  await new Promise((resolve) => extra.addEventListener('open', resolve, { once: true }));
  extra.send(JSON.stringify({ type: 'join', protocol: 1, compatibility: 1, invite: ready.invite }));
  const rejected = JSON.parse(await nextMessage(extra));
  assert.equal(rejected.code, 'full');

  for (const socket of sockets) socket.close();
  await relay.close();
});

test('malformed binary relay data is closed without affecting another room', async () => {
  const relay = createRelayServer({ host: '127.0.0.1', port: 0 });
  const address = await relay.listen();
  const url = `ws://127.0.0.1:${address.port}/tiberian-dawn-relay`;
  const attacker = new WebSocket(url);
  await new Promise((resolve) => attacker.addEventListener('open', resolve, { once: true }));
  attacker.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 1 }));
  assert.equal(JSON.parse(await nextMessage(attacker)).type, 'ready');
  const closed = new Promise((resolve) => attacker.addEventListener('close', resolve, { once: true }));
  attacker.send(Buffer.from('not-a-relay-packet'));
  await closed;

  const healthy = new WebSocket(url);
  await new Promise((resolve) => healthy.addEventListener('open', resolve, { once: true }));
  healthy.send(JSON.stringify({ type: 'create', protocol: 1, compatibility: 1 }));
  assert.equal(JSON.parse(await nextMessage(healthy)).type, 'ready');
  healthy.close();
  await relay.close();
});
