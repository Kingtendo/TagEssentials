import crypto from 'node:crypto';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const UUID_RE = /^[0-9a-f]{32}$/;
const USERNAME_RE = /^[A-Za-z0-9_]{1,16}$/;

function envInteger(name, fallback, minimum, maximum) {
  const parsed = Number.parseInt(process.env[name] ?? '', 10);
  if (!Number.isFinite(parsed)) return fallback;
  return Math.max(minimum, Math.min(maximum, parsed));
}

export function normalizeUuid(value) {
  const normalized = String(value ?? '').trim().replaceAll('-', '').toLowerCase();
  return UUID_RE.test(normalized) ? normalized : '';
}

export function isMinecraftUsername(value) {
  return USERNAME_RE.test(String(value ?? ''));
}

function safeTokenEqual(actual, expected) {
  const actualBuffer = Buffer.from(String(actual ?? ''), 'utf8');
  const expectedBuffer = Buffer.from(String(expected ?? ''), 'utf8');
  if (actualBuffer.length !== expectedBuffer.length || expectedBuffer.length === 0) return false;
  return crypto.timingSafeEqual(actualBuffer, expectedBuffer);
}

function jsonResponse(response, status, body, extraHeaders = {}) {
  const payload = Buffer.from(JSON.stringify(body));
  response.writeHead(status, {
    'Cache-Control': 'no-store',
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': payload.length,
    'X-Content-Type-Options': 'nosniff',
    'Referrer-Policy': 'no-referrer',
    ...extraHeaders
  });
  response.end(payload);
}

export class FixedWindowLimiter {
  constructor({ limit, windowMs, now = () => Date.now() }) {
    this.limit = limit;
    this.windowMs = windowMs;
    this.now = now;
    this.windows = new Map();
  }

  take(key) {
    const now = this.now();
    let entry = this.windows.get(key);
    if (!entry || now >= entry.resetAt) {
      entry = { count: 0, resetAt: now + this.windowMs };
      this.windows.set(key, entry);
    }
    if (entry.count >= this.limit) {
      return { allowed: false, retryAfterMs: Math.max(1000, entry.resetAt - now) };
    }
    entry.count += 1;
    if (this.windows.size > 10000) {
      for (const [candidate, candidateEntry] of this.windows) {
        if (now >= candidateEntry.resetAt) this.windows.delete(candidate);
      }
    }
    return { allowed: true, retryAfterMs: 0 };
  }
}

export class SlidingWindowBudget {
  constructor({ limit, windowMs, now = () => Date.now() }) {
    this.limit = limit;
    this.windowMs = windowMs;
    this.now = now;
    this.timestamps = [];
    this.blockedUntil = 0;
  }

  blockFor(delayMs) {
    this.blockedUntil = Math.max(this.blockedUntil, this.now() + Math.max(1000, delayMs));
  }

  reserve() {
    const now = this.now();
    if (now < this.blockedUntil) return { allowed: false, retryAfterMs: this.blockedUntil - now };
    while (this.timestamps.length > 0 && now - this.timestamps[0] >= this.windowMs) this.timestamps.shift();
    if (this.timestamps.length >= this.limit) {
      return { allowed: false, retryAfterMs: Math.max(1000, this.timestamps[0] + this.windowMs - now) };
    }
    this.timestamps.push(now);
    return { allowed: true, retryAfterMs: 0 };
  }
}

export class PersistentWinsCache {
  constructor({ file, staleTtlMs, now = () => Date.now() }) {
    this.file = file;
    this.staleTtlMs = staleTtlMs;
    this.now = now;
    this.entries = new Map();
    this.savePromise = Promise.resolve();
    this.saveScheduled = false;
    this.dirty = false;
  }

  async load() {
    try {
      const parsed = JSON.parse(await fs.readFile(this.file, 'utf8'));
      if (parsed?.version !== 1 || !Array.isArray(parsed.entries)) return;
      const now = this.now();
      for (const item of parsed.entries) {
        const uuid = normalizeUuid(item?.uuid);
        const fetchedAt = Number(item?.fetchedAt);
        const wins = Number(item?.wins);
        const displayName = String(item?.displayName ?? '');
        if (!uuid || !Number.isSafeInteger(fetchedAt) || fetchedAt <= 0 || fetchedAt > now + 300000) continue;
        if (now - fetchedAt > this.staleTtlMs) continue;
        if (item.available && (!isMinecraftUsername(displayName) || !Number.isSafeInteger(wins) || wins < 0)) continue;
        this.entries.set(uuid, {
          available: Boolean(item.available),
          wins: item.available ? wins : 0,
          displayName: item.available ? displayName : '',
          fetchedAt
        });
      }
    } catch (error) {
      if (error?.code !== 'ENOENT') console.error('cache load failed:', error.message);
    }
  }

  get(uuid) {
    const entry = this.entries.get(uuid);
    if (!entry) return null;
    if (this.now() - entry.fetchedAt > this.staleTtlMs) {
      this.entries.delete(uuid);
      return null;
    }
    return entry;
  }

  set(uuid, entry) {
    this.entries.set(uuid, entry);
    this.scheduleSave();
  }

  scheduleSave() {
    this.dirty = true;
    if (this.saveScheduled) return this.savePromise;
    this.saveScheduled = true;
    this.savePromise = this.savePromise.then(async () => {
      await new Promise((resolve) => setTimeout(resolve, 100));
      while (this.dirty) {
        this.dirty = false;
        const directory = path.dirname(this.file);
        const temporary = `${this.file}.tmp`;
        await fs.mkdir(directory, { recursive: true });
        const entries = [...this.entries.entries()].map(([uuid, entry]) => ({ uuid, ...entry }));
        await fs.writeFile(temporary, `${JSON.stringify({ version: 1, entries })}\n`, { mode: 0o600 });
        await fs.rename(temporary, this.file);
      }
      this.saveScheduled = false;
    }).catch((error) => {
      this.saveScheduled = false;
      console.error('cache save failed:', error.message);
    });
    return this.savePromise;
  }
}

export class WinsService {
  constructor({ apiKey, cache, budget, fetchImpl = fetch, cacheTtlMs, staleTtlMs, now = () => Date.now() }) {
    this.apiKey = apiKey;
    this.cache = cache;
    this.budget = budget;
    this.fetchImpl = fetchImpl;
    this.cacheTtlMs = cacheTtlMs;
    this.staleTtlMs = staleTtlMs;
    this.now = now;
    this.inFlight = new Map();
    this.upstreamStatus = 'not_checked';
    this.lastUpstreamSuccessAt = 0;
  }

  resultForName(entry, playerName, cacheStatus, retryAfterMs = 0) {
    const matches = entry.available && entry.displayName.toLowerCase() === playerName.toLowerCase();
    return {
      status: 200,
      body: {
        ok: true,
        available: matches,
        ...(matches ? { wins: entry.wins } : {}),
        fetched_at: entry.fetchedAt,
        cache_status: cacheStatus,
        ...(retryAfterMs > 0 ? { retry_after_seconds: Math.ceil(retryAfterMs / 1000) } : {})
      }
    };
  }

  async lookup(uuid, playerName) {
    const now = this.now();
    const cached = this.cache.get(uuid);
    if (cached && now - cached.fetchedAt < this.cacheTtlMs) return this.resultForName(cached, playerName, 'hit');

    let pending = this.inFlight.get(uuid);
    if (!pending) {
      const reservation = this.budget.reserve();
      if (!reservation.allowed) {
        if (cached) return this.resultForName(cached, playerName, 'stale', reservation.retryAfterMs);
        return {
          status: 503,
          retryAfterMs: reservation.retryAfterMs,
          body: { ok: false, error: 'upstream_budget_exhausted', retry_after_seconds: Math.ceil(reservation.retryAfterMs / 1000) }
        };
      }
      pending = this.fetchPlayer(uuid).finally(() => this.inFlight.delete(uuid));
      this.inFlight.set(uuid, pending);
    }

    try {
      const fresh = await pending;
      return this.resultForName(fresh, playerName, 'miss');
    } catch (error) {
      const retryAfterMs = Math.max(2000, Number(error?.retryAfterMs) || 120000);
      if (cached) return this.resultForName(cached, playerName, 'stale', retryAfterMs);
      return {
        status: error?.forbidden ? 503 : 502,
        retryAfterMs,
        body: { ok: false, error: error?.publicCode || 'upstream_unavailable', retry_after_seconds: Math.ceil(retryAfterMs / 1000) }
      };
    }
  }

  async fetchPlayer(uuid) {
    let response;
    try {
      response = await this.fetchImpl(`https://api.hypixel.net/v2/player?uuid=${uuid}`, {
        headers: { Accept: 'application/json', 'API-Key': this.apiKey },
        signal: AbortSignal.timeout(8000)
      });
    } catch (cause) {
      this.upstreamStatus = 'unreachable';
      throw Object.assign(new Error('Hypixel request failed', { cause }), { publicCode: 'upstream_unavailable' });
    }

    const resetSeconds = Number.parseInt(response.headers.get('ratelimit-reset') ?? '', 10);
    if (response.status === 429) {
      const retryAfterMs = Number.isFinite(resetSeconds) && resetSeconds > 0 ? resetSeconds * 1000 : 120000;
      this.budget.blockFor(retryAfterMs);
      this.upstreamStatus = 'rate_limited';
      throw Object.assign(new Error('Hypixel rate limit reached'), { retryAfterMs, publicCode: 'upstream_rate_limited' });
    }
    if (response.status === 401 || response.status === 403) {
      this.budget.blockFor(300000);
      this.upstreamStatus = 'credentials_rejected';
      console.error('Hypixel rejected the configured API credentials');
      throw Object.assign(new Error('Hypixel credentials rejected'), {
        retryAfterMs: 300000,
        publicCode: 'upstream_credentials_rejected',
        forbidden: true
      });
    }
    if (!response.ok) {
      this.upstreamStatus = `http_${response.status}`;
      throw Object.assign(new Error(`Hypixel returned ${response.status}`), { publicCode: 'upstream_unavailable' });
    }

    let payload;
    try {
      payload = await response.json();
    } catch (cause) {
      this.upstreamStatus = 'invalid_response';
      throw Object.assign(new Error('Hypixel returned invalid JSON', { cause }), { publicCode: 'upstream_invalid_response' });
    }
    if (payload?.success !== true) {
      this.upstreamStatus = 'invalid_response';
      throw Object.assign(new Error('Hypixel request was unsuccessful'), { publicCode: 'upstream_invalid_response' });
    }

    const fetchedAt = this.now();
    let entry;
    if (!payload.player) {
      entry = { available: false, wins: 0, displayName: '', fetchedAt };
    } else {
      const displayName = String(payload.player.displayname ?? '');
      if (!isMinecraftUsername(displayName)) {
        throw Object.assign(new Error('Hypixel player name was invalid'), { publicCode: 'upstream_invalid_response' });
      }
      const rawWins = payload.player?.stats?.TNTGames?.wins_tntag ?? 0;
      const wins = Number(rawWins);
      if (!Number.isSafeInteger(wins) || wins < 0) {
        throw Object.assign(new Error('Hypixel TNT Tag wins were invalid'), { publicCode: 'upstream_invalid_response' });
      }
      entry = { available: true, wins, displayName, fetchedAt };
    }
    this.cache.set(uuid, entry);
    this.upstreamStatus = 'ok';
    this.lastUpstreamSuccessAt = fetchedAt;
    return entry;
  }
}

export function createRequestHandler({ service, clientToken, clientLimiter, globalLimiter, trustProxy }) {
  return async (request, response) => {
    if (request.method === 'GET' && request.url === '/healthz') {
      jsonResponse(response, 200, {
        ok: true,
        cache_entries: service.cache.entries.size,
        upstream_requests_in_flight: service.inFlight.size,
        upstream_status: service.upstreamStatus,
        last_upstream_success_at: service.lastUpstreamSuccessAt || null
      });
      return;
    }

    let url;
    try {
      url = new URL(request.url, 'http://localhost');
    } catch {
      jsonResponse(response, 400, { ok: false, error: 'invalid_request' });
      return;
    }
    if (request.method !== 'GET' || url.pathname !== '/v1/tnttag/wins') {
      jsonResponse(response, 404, { ok: false, error: 'not_found' });
      return;
    }

    if (!safeTokenEqual(request.headers['x-public-helpers-token'], clientToken)) {
      jsonResponse(response, 401, { ok: false, error: 'unauthorized' });
      return;
    }

    const forwarded = trustProxy ? String(request.headers['x-forwarded-for'] ?? '').split(',')[0].trim() : '';
    const clientIp = forwarded || request.socket.remoteAddress || 'unknown';
    const clientLimit = clientLimiter.take(clientIp);
    const globalLimit = globalLimiter.take('global');
    const retryAfterMs = Math.max(clientLimit.retryAfterMs, globalLimit.retryAfterMs);
    if (!clientLimit.allowed || !globalLimit.allowed) {
      jsonResponse(response, 429, {
        ok: false,
        error: 'client_rate_limited',
        retry_after_seconds: Math.ceil(retryAfterMs / 1000)
      }, { 'Retry-After': String(Math.ceil(retryAfterMs / 1000)) });
      return;
    }

    const uuid = normalizeUuid(url.searchParams.get('uuid'));
    const playerName = url.searchParams.get('name') ?? '';
    if (!uuid || !isMinecraftUsername(playerName)) {
      jsonResponse(response, 400, { ok: false, error: 'invalid_player' });
      return;
    }

    const result = await service.lookup(uuid, playerName);
    const headers = {};
    if (result.retryAfterMs > 0) headers['Retry-After'] = String(Math.ceil(result.retryAfterMs / 1000));
    jsonResponse(response, result.status, result.body, headers);
  };
}

export async function startServer() {
  const apiKey = String(process.env.HYPIXEL_API_KEY ?? '').trim();
  const clientToken = String(process.env.PUBLIC_HELPERS_TOKEN ?? '').trim();
  if (!apiKey) throw new Error('HYPIXEL_API_KEY is required');
  if (clientToken.length < 24) throw new Error('PUBLIC_HELPERS_TOKEN must be at least 24 characters');

  const port = envInteger('PORT', 8080, 1, 65535);
  const host = String(process.env.HOST ?? '127.0.0.1');
  const cacheTtlMs = envInteger('CACHE_TTL_MS', 3600000, 3600000, 604800000);
  const staleTtlMs = envInteger('STALE_TTL_MS', 86400000, cacheTtlMs, 2592000000);
  const rateWindowMs = envInteger('RATE_WINDOW_MS', 300000, 60000, 3600000);
  const cacheFile = String(process.env.CACHE_FILE ?? '/data/tnttag-wins-cache.json');
  const now = () => Date.now();

  const cache = new PersistentWinsCache({ file: cacheFile, staleTtlMs, now });
  await cache.load();
  const budget = new SlidingWindowBudget({
    limit: envInteger('HYPIXEL_REQUEST_LIMIT', 280, 1, 100000),
    windowMs: rateWindowMs,
    now
  });
  const service = new WinsService({ apiKey, cache, budget, cacheTtlMs, staleTtlMs, now });
  const clientLimiter = new FixedWindowLimiter({
    limit: envInteger('CLIENT_REQUEST_LIMIT', 240, 1, 100000),
    windowMs: rateWindowMs,
    now
  });
  const globalLimiter = new FixedWindowLimiter({
    limit: envInteger('GLOBAL_REQUEST_LIMIT', 10000, 1, 1000000),
    windowMs: rateWindowMs,
    now
  });
  const handler = createRequestHandler({
    service,
    clientToken,
    clientLimiter,
    globalLimiter,
    trustProxy: String(process.env.TRUST_PROXY ?? '1') === '1'
  });
  const server = http.createServer((request, response) => {
    handler(request, response).catch((error) => {
      console.error('request failed:', error.message);
      if (!response.headersSent) jsonResponse(response, 500, { ok: false, error: 'internal_error' });
      else response.destroy();
    });
  });
  server.requestTimeout = 12000;
  server.headersTimeout = 5000;
  server.keepAliveTimeout = 5000;
  server.listen(port, host, () => console.log(`Public Helpers server listening on http://${host}:${port}`));

  const shutdown = async (signal) => {
    console.log(`${signal} received; shutting down`);
    server.close();
    await cache.scheduleSave();
    process.exit(0);
  };
  process.once('SIGTERM', () => void shutdown('SIGTERM'));
  process.once('SIGINT', () => void shutdown('SIGINT'));
  return server;
}

const isEntryPoint = process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url);
if (isEntryPoint) startServer().catch((error) => {
  console.error(error.message);
  process.exit(1);
});
