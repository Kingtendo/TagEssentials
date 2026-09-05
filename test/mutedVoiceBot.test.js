const assert = require('node:assert/strict');
const { EventEmitter } = require('node:events');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

const scriptPath = path.join(__dirname, '..', 'mutedVoiceBot.js');
const source = fs.readFileSync(scriptPath, 'utf8');

function createHarness({ failFirst = false, quitThrows = false } = {}) {
  const bots = [];
  const children = [];
  const timers = new Set();
  let acceptSocket;
  const server = new EventEmitter();
  server.listen = (_port, _host, callback) => callback();
  server.close = (callback) => callback?.();
  const fakeProcess = new EventEmitter();
  Object.assign(fakeProcess, { argv: ['node', scriptPath], env: {}, platform: 'win32', exit() {} });
  const modules = {
    fs: {
      appendFileSync() {}, existsSync: () => true, mkdirSync() {}, rmSync() {},
      readFileSync: () => JSON.stringify({ username: 'TestAccount', partyOwnerUsername: 'Owner' })
    },
    net: { createServer(callback) { acceptSocket = callback; return server; } },
    path,
    child_process: {
      spawn() {
        const child = new EventEmitter();
        child.unref = () => {};
        children.push(child);
        return child;
      }
    },
    mineflayer: {
      createBot(options) {
        if (failFirst) { failFirst = false; throw new Error('bad connection settings'); }
        const bot = new EventEmitter();
        Object.assign(bot, {
          options, sent: [], cleared: 0,
          clearControlStates() { this.cleared += 1; },
          chat(message) { this.sent.push(message); },
          quit() { if (quitThrows) throw new Error('quit failed'); },
          end() {}
        });
        bots.push(bot);
        return bot;
      }
    }
  };
  const context = vm.createContext({
    require(name) { assert.ok(name in modules, `unexpected module ${name}`); return modules[name]; },
    __dirname: path.dirname(scriptPath), process: fakeProcess,
    setTimeout(callback) {
      const timer = { callback, unref() {} };
      timers.add(timer);
      return timer;
    },
    clearTimeout(timer) { timers.delete(timer); }
  });
  vm.runInContext(`${source}\n;globalThis.testApi = {
    createBot, stopBot, queueChat, openExternalUrl, openTextFile,
    get bot() { return bot; }, get status() { return status; },
    get auth() { return latestAuthPrompt; }, get queued() { return chatQueue.length; }
  };`, context, { filename: scriptPath });
  return { api: context.testApi, bots, children, timers, acceptSocket: (socket) => acceptSocket(socket) };
}

test('late events from a signed out bot cannot overwrite a new connection', () => {
  const { api, bots, children } = createHarness();
  api.createBot();
  const old = bots[0];
  api.stopBot(true);
  api.createBot();
  const current = bots[1];
  current.emit('login');
  old.emit('end', 'old connection ended');
  old.emit('spawn');
  old.emit('error', new Error('old connection failed'));
  old.options.onMsaCode({ user_code: 'OLD' });
  assert.equal(api.bot, current);
  assert.equal(api.status, 'online');
  assert.equal(api.auth, null);
  assert.equal(children.length, 0);
  assert.equal(current.cleared, 0);
});

test('connection creation errors produce a recoverable error status', () => {
  const { api, bots } = createHarness({ failFirst: true });
  assert.doesNotThrow(() => api.createBot());
  assert.equal(api.status, 'error');
  assert.equal(api.bot, null);
  api.createBot();
  bots[0].emit('login');
  assert.equal(api.status, 'online');
});

test('a pre login connection failure retries once through the fallback host', () => {
  const { api, bots } = createHarness();
  api.createBot();
  const primary = bots[0];
  assert.equal(primary.options.host, 'mc.hypixel.net');

  primary.emit('error', new Error('read ECONNRESET'));
  assert.equal(bots.length, 2);
  assert.equal(bots[1].options.host, 'chi.free.overlag.link');
  assert.equal(api.bot, bots[1]);
  assert.equal(api.status, 'connecting');

  primary.emit('end', 'socketClosed');
  assert.equal(api.bot, bots[1]);
  bots[1].emit('login');
  assert.equal(api.status, 'online');
});

test('the fallback host is attempted only once', () => {
  const { api, bots } = createHarness();
  api.createBot();
  bots[0].emit('end', 'socketClosed');
  assert.equal(bots.length, 2);
  bots[1].emit('end', 'socketClosed');
  assert.equal(bots.length, 2);
  assert.equal(api.bot, null);
  assert.equal(api.status, 'offline');
});

test('a disconnect after login does not switch hosts', () => {
  const { api, bots } = createHarness();
  api.createBot();
  bots[0].emit('login');
  bots[0].emit('end', 'socketClosed');
  assert.equal(bots.length, 1);
  assert.equal(api.bot, null);
  assert.equal(api.status, 'offline');
});

test('a server kick does not switch hosts', () => {
  const { api, bots } = createHarness();
  api.createBot();
  bots[0].emit('kicked', 'banned');
  bots[0].emit('end', 'socketClosed');
  assert.equal(bots.length, 1);
  assert.equal(api.bot, null);
});

test('disconnect clears queued messages, timers and authentication prompts', () => {
  const { api, bots, timers } = createHarness();
  api.createBot();
  bots[0].options.onMsaCode({ user_code: 'CODE' });
  assert.ok(api.auth);
  bots[0].emit('login');
  assert.equal(api.queueChat('hello'), true);
  assert.equal(api.queued, 1);
  assert.equal(timers.size, 1);
  api.stopBot(true);
  assert.equal(api.status, 'offline');
  assert.equal(api.bot, null);
  assert.equal(api.queued, 0);
  assert.equal(api.auth, null);
  assert.equal(timers.size, 0);
});

test('a failed graceful quit permits reconnect and ignores the old bot', () => {
  const { api, bots } = createHarness({ quitThrows: true });
  api.createBot();
  api.stopBot();
  assert.equal(api.bot, null);
  api.createBot();
  bots[1].emit('login');
  bots[0].emit('end');
  assert.equal(api.bot, bots[1]);
  assert.equal(api.status, 'online');
});

test('browser and editor spawn errors have handlers', () => {
  const { api, children } = createHarness();
  api.openExternalUrl('https://www.microsoft.com/link');
  api.openTextFile('config.json');
  for (const child of children) assert.doesNotThrow(() => child.emit('error', new Error('not found')));
});

test('control connections cannot buffer unlimited unterminated input', () => {
  const { acceptSocket } = createHarness();
  const socket = new EventEmitter();
  Object.assign(socket, { destroyed: false, setEncoding() {}, write() {}, destroy() { this.destroyed = true; } });
  acceptSocket(socket);
  socket.emit('data', 'x'.repeat(65537));
  assert.equal(socket.destroyed, true);
});
