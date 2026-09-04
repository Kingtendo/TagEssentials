import assert from 'node:assert/strict';
import { once } from 'node:events';
import http from 'node:http';
import test from 'node:test';

import {
  FixedWindowLimiter,
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

test('normalizes UUIDs and validates Minecraft usernames', () => {
  assert.equal(normalizeUuid('12345678-90AB-CDEF-1234-567890ABCDEF'), UUID);
  assert.equal(normalizeUuid('not-a-uuid'), '');
  assert.equal(isMinecraftUsername('Mike_123'), true);
  assert.equal(isMinecraftUsername('bad name'), false);
});

test('deduplicates simultaneous upstream requests and caches globally by UUID', async () => {
  let fetches = 0;
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
  assert.equal('wins' in nickMismatch.body, false);
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
    fetchImpl: async () => hypixelResponse(responsePlayer)
  });
  const zero = await service.lookup(UUID, 'ZeroWins');
  assert.equal(zero.body.available, true);
  assert.equal(zero.body.wins, 0);

  responsePlayer = null;
  now += 3600001;
  const missing = await service.lookup(UUID, 'ZeroWins');
  assert.equal(missing.body.available, false);
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

test('fixed-window limiter isolates clients', () => {
  let now = 1000;
  const limiter = new FixedWindowLimiter({ limit: 2, windowMs: 5000, now: () => now });
  assert.equal(limiter.take('a').allowed, true);
  assert.equal(limiter.take('a').allowed, true);
  assert.equal(limiter.take('a').allowed, false);
  assert.equal(limiter.take('b').allowed, true);
  now += 5000;
  assert.equal(limiter.take('a').allowed, true);
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
