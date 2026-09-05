import assert from 'node:assert/strict';
import { once } from 'node:events';
import http from 'node:http';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';

import {
  FixedWindowLimiter,
  PersistentWinsCache,
  SlidingWindowBudget,
  WinsService,
  createRequestHandler,
  isMinecraftUsername,
  normalizeUuid
} from '../server.js';

const UUID = '1234567890abcdef1234567890abcdef';

class MemoryCache {
  constructor() {
    this.entries = new Map();
  }
  get(uuid) {
    return this.entries.get(uuid) ?? null;
  }
  set(uuid, entry) {
    this.entries.set(uuid, entry);
  }
}

function hypixelResponse(player, status = 200, headers = {}) {
  return {
    ok: status >= 200 && status < 300,
    status,
    headers: new Headers(headers),
    async json() {
      return { success: true, player };
    }
  };
}

function minecraftProfileResponse(profile, status = 200) {
  return {
    ok: status >= 200 && status < 300,
    status,
    async json() {
      return profile;
    }
  };
}

test('normalizes UUIDs and validates Minecraft usernames', () => {
  assert.equal(normalizeUuid('12345678-90AB-CDEF-1234-567890ABCDEF'), UUID);
  assert.equal(normalizeUuid('not-a-uuid'), '');
  assert.equal(isMinecraftUsername('Mike_123'), true);
  assert.equal(isMinecraftUsername('bad name'), false);
});

test('deduplicates simultaneous upstream requests and caches globally by UUID', async () => {
  let fetches = 0;
  let identityChecks = 0;
  let now = 1_700_000_000_000;
  const cache = new MemoryCache();
  const service = new WinsService({
    apiKey: 'test',
    cache,
    budget: new SlidingWindowBudget({ limit: 10, windowMs: 300000, now: () => now }),
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => {
      fetches += 1;
      await new Promise((resolve) => setTimeout(resolve, 10));
      return hypixelResponse({ displayname: 'Mike', stats: { TNTGames: { wins_tntag: 5504 } } });
    },
    identityFetchImpl: async () => {
      identityChecks += 1;
      return minecraftProfileResponse({ id: UUID, name: 'Mike' });
    }
  });

  const [first, second] = await Promise.all([
    service.lookup(UUID, 'Mike'),
    service.lookup(UUID, 'Mike')
  ]);
  assert.equal(fetches, 1);
  assert.equal(first.body.available, true);
  assert.equal(second.body.wins, 5504);

  const cached = await service.lookup(UUID, 'Mike');
  assert.equal(fetches, 1);
  assert.equal(cached.body.cache_status, 'hit');

  const nickMismatch = await service.lookup(UUID, 'NotMike');
  assert.equal(nickMismatch.body.available, false);
  assert.equal(nickMismatch.body.nicked, true);
  assert.equal('wins' in nickMismatch.body, false);
  assert.equal(fetches, 1, 'fresh Hypixel cache should be reused');
  assert.equal(identityChecks, 1, 'the name mismatch must be independently confirmed');
});

test('caches zero wins and missing players as definitive results', async () => {
  let now = 1_700_000_000_000;
  let responsePlayer = { displayname: 'ZeroWins', stats: { TNTGames: {} } };
  const service = new WinsService({
    apiKey: 'test',
    cache: new MemoryCache(),
    budget: new SlidingWindowBudget({ limit: 10, windowMs: 300000, now: () => now }),
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => hypixelResponse(responsePlayer),
    identityFetchImpl: async () => minecraftProfileResponse({ id: UUID, name: 'ZeroWins' })
  });
  const zero = await service.lookup(UUID, 'ZeroWins');
  assert.equal(zero.body.available, true);
  assert.equal(zero.body.wins, 0);

  responsePlayer = null;
  now += 3600001;
  const missing = await service.lookup(UUID, 'ZeroWins');
  assert.equal(missing.body.available, false);
  assert.equal(missing.body.nicked, false);
});

test('labels a synthetic in game UUID as nicked only after Minecraft confirms it', async () => {
  const now = 1_700_000_000_000;
  let hypixelFetches = 0;
  let identityChecks = 0;
  const service = new WinsService({
    apiKey: 'test',
    cache: new MemoryCache(),
    budget: new SlidingWindowBudget({ limit: 10, windowMs: 300000, now: () => now }),
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => {
      hypixelFetches += 1;
      return hypixelResponse(null);
    },
    identityFetchImpl: async () => {
      identityChecks += 1;
      return minecraftProfileResponse(null, 204);
    }
  });

  const result = await service.lookup(UUID, 'TheLuna273');
  assert.equal(result.status, 200);
  assert.equal(result.body.available, false);
  assert.equal(result.body.nicked, true);
  assert.equal('wins' in result.body, false);

  const cached = await service.lookup(UUID, 'TheLuna273');
  assert.equal(cached.body.nicked, true);
  assert.equal(cached.body.cache_status, 'hit');
  assert.equal(hypixelFetches, 1);
  assert.equal(identityChecks, 1);
});

test('never labels a missing Hypixel profile when Minecraft confirms the visible player', async () => {
  const now = 1_700_000_000_000;
  const service = new WinsService({
    apiKey: 'test',
    cache: new MemoryCache(),
    budget: new SlidingWindowBudget({ limit: 10, windowMs: 300000, now: () => now }),
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => hypixelResponse(null),
    identityFetchImpl: async () => minecraftProfileResponse({ id: UUID, name: 'RealPlayer' })
  });

  const result = await service.lookup(UUID, 'RealPlayer');
  assert.equal(result.status, 200);
  assert.equal(result.body.available, false);
  assert.equal(result.body.nicked, false);
});

test('treats an unavailable Minecraft identity check as transient, not nicked', async () => {
  const now = 1_700_000_000_000;
  const service = new WinsService({
    apiKey: 'test',
    cache: new MemoryCache(),
    budget: new SlidingWindowBudget({ limit: 10, windowMs: 300000, now: () => now }),
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => hypixelResponse(null),
    identityFetchImpl: async () => minecraftProfileResponse(null, 503)
  });

  const result = await service.lookup(UUID, 'RealPlayer');
  assert.equal(result.status, 502);
  assert.equal(result.body.ok, false);
  assert.equal('nicked' in result.body, false);
});

test('serves stale cache without consuming an exhausted upstream budget', async () => {
  let now = 1_700_000_000_000;
  const cache = new MemoryCache();
  cache.set(UUID, { available: true, wins: 2500, displayName: 'Mike', fetchedAt: now - 3600001 });
  const budget = new SlidingWindowBudget({ limit: 1, windowMs: 300000, now: () => now });
  assert.equal(budget.reserve().allowed, true);
  const service = new WinsService({
    apiKey: 'test',
    cache,
    budget,
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => assert.fail('upstream fetch should not run')
  });

  const result = await service.lookup(UUID, 'Mike');
  assert.equal(result.status, 200);
  assert.equal(result.body.cache_status, 'stale');
  assert.equal(result.body.wins, 2500);
  assert.ok(result.body.retry_after_seconds > 0);
});

test('never labels a cached name mismatch when identity confirmation is unavailable', async () => {
  const now = 1_700_000_000_000;
  const cache = new MemoryCache();
  cache.set(UUID, { available: true, wins: 2500, displayName: 'Mike', fetchedAt: now });
  const budget = new SlidingWindowBudget({ limit: 1, windowMs: 300000, now: () => now });
  assert.equal(budget.reserve().allowed, true);
  const service = new WinsService({
    apiKey: 'test',
    cache,
    budget,
    cacheTtlMs: 3600000,
    staleTtlMs: 86400000,
    now: () => now,
    fetchImpl: async () => assert.fail('Hypixel fetch should not run'),
    identityFetchImpl: async () => minecraftProfileResponse(null, 503)
  });

  const result = await service.lookup(UUID, 'NotMike');
  assert.equal(result.status, 502);
  assert.equal(result.body.ok, false);
  assert.equal('nicked' in result.body, false);
});

test('fixed window limiter isolates clients', () => {
  let now = 1000;
  const limiter = new FixedWindowLimiter({ limit: 2, windowMs: 5000, now: () => now });
  assert.equal(limiter.take('a').allowed, true);
  assert.equal(limiter.take('a').allowed, true);
  assert.equal(limiter.take('a').allowed, false);
  assert.equal(limiter.take('b').allowed, true);
  now += 5000;
  assert.equal(limiter.take('a').allowed, true);
});

test('rejected client requests cannot consume the shared request budget', async () => {
  const handler = createRequestHandler({
    service: { async lookup() { return { status: 200, body: { ok: true } }; } },
    clientToken: 'test-token',
    clientLimiter: new FixedWindowLimiter({ limit: 1, windowMs: 300000 }),
    globalLimiter: new FixedWindowLimiter({ limit: 2, windowMs: 300000 }),
    trustProxy: false
  });
  async function request(ip) {
    const response = { writeHead(status) { this.status = status; }, end() {} };
    await handler({
      method: 'GET', url: `/v1/tnttag/wins?uuid=${UUID}&name=Mike`,
      headers: { 'x-public-helpers-token': 'test-token' }, socket: { remoteAddress: ip }
    }, response);
    return response.status;
  }
  assert.equal(await request('client-a'), 200);
  for (let i = 0; i < 5; i += 1) assert.equal(await request('client-a'), 429);
  assert.equal(await request('client-b'), 200);
  assert.equal(await request('client-c'), 429);
});

test('cache saves evict expired players even when nobody looks them up again', async (context) => {
  const directory = await fs.mkdtemp(path.join(os.tmpdir(), 'tagessentials-cache-test-'));
  context.after(() => fs.rm(directory, { recursive: true, force: true }));
  let now = 10000;
  const file = path.join(directory, 'cache.json');
  const cache = new PersistentWinsCache({ file, staleTtlMs: 1000, now: () => now });
  cache.set(UUID, { available: true, wins: 10, displayName: 'Mike', fetchedAt: now });
  await cache.savePromise;
  now += 2000;
  const otherUuid = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';
  cache.set(otherUuid, { available: true, wins: 20, displayName: 'Alex', fetchedAt: now });
  await cache.savePromise;
  assert.equal(cache.entries.has(UUID), false);
  const saved = JSON.parse(await fs.readFile(file, 'utf8'));
  assert.deepEqual(saved.entries.map((entry) => entry.uuid), [otherUuid]);
  const reloaded = new PersistentWinsCache({ file, staleTtlMs: 1000, now: () => now });
  await reloaded.load();
  assert.equal(reloaded.get(otherUuid).wins, 20);
});

test('HTTP route requires the app token and exposes only the wins contract', async (context) => {
  const service = {
    cache: { entries: new Map() },
    inFlight: new Map(),
    async lookup(uuid, name) {
      assert.equal(uuid, UUID);
      assert.equal(name, 'Mike');
      return { status: 200, body: { ok: true, available: true, wins: 5504, fetched_at: 1700000000000 } };
    }
  };
  const handler = createRequestHandler({
    service,
    clientToken: '12345678901234567890123456789012',
    clientLimiter: new FixedWindowLimiter({ limit: 10, windowMs: 300000 }),
    globalLimiter: new FixedWindowLimiter({ limit: 10, windowMs: 300000 }),
    trustProxy: false
  });
  const server = http.createServer(handler);
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  context.after(() => server.close());
  const { port } = server.address();

  const unauthorized = await fetch(`http://127.0.0.1:${port}/v1/tnttag/wins?uuid=${UUID}&name=Mike`);
  assert.equal(unauthorized.status, 401);

  const response = await fetch(`http://127.0.0.1:${port}/v1/tnttag/wins?uuid=${UUID}&name=Mike`, {
    headers: { 'X-Public-Helpers-Token': '12345678901234567890123456789012' }
  });
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), {
    ok: true,
    available: true,
    wins: 5504,
    fetched_at: 1700000000000
  });

  const genericProxyAttempt = await fetch(`http://127.0.0.1:${port}/v2/player?uuid=${UUID}`);
  assert.equal(genericProxyAttempt.status, 404);
});
