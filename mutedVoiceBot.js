const fs = require('fs');
const net = require('net');
const path = require('path');
const { spawn } = require('child_process');

const DEFAULT_CONTROL_PORT = 49623;
const DEFAULT_CONFIG = {
  host: 'mc.hypixel.net',
  fallbackHost: process.env.MUTED_VOICE_FALLBACK_HOST || 'chi.free.overlag.link',
  minecraftPort: 25565,
  version: '1.8.9',
  auth: 'microsoft',
  flow: 'live',
  username: process.env.MUTED_VOICE_USERNAME || process.env.MC_USERNAME || '',
  partyOwnerUsername: process.env.MUTED_VOICE_PARTY_OWNER_USERNAME || process.env.MUTED_VOICE_PARTY_OWNER || '',
  profilesFolder: path.join(__dirname, '.mineflayer-auth'),
  chatDelayMs: Number(process.env.MUTED_VOICE_CHAT_DELAY_MS || 900),
  partyAcceptDelayMs: Number(process.env.MUTED_VOICE_PARTY_ACCEPT_DELAY_MS || 250),
  maxChatQueue: Number(process.env.MUTED_VOICE_MAX_CHAT_QUEUE || 32)
};

const args = process.argv.slice(2);
let controlPort = Number(process.env.MUTED_VOICE_CONTROL_PORT || DEFAULT_CONTROL_PORT);
for (let i = 0; i < args.length; i += 1) {
  if (args[i] === '--control-port' && args[i + 1]) {
    controlPort = Number(args[i + 1]);
    i += 1;
  }
}
if (!Number.isInteger(controlPort) || controlPort <= 0 || controlPort > 65535) {
  controlPort = DEFAULT_CONTROL_PORT;
}

const logPath = path.join(__dirname, 'mutedVoiceBot.log');
function log(message) {
  const line = `[${new Date().toISOString()}] ${message}\n`;
  try {
    fs.appendFileSync(logPath, line);
  } catch (_) {
    // Logging must never break the control server.
  }
}
function openDesktopTarget(target, isUrl = false) {
  if (typeof target !== 'string' || !target) return false;
  const label = isUrl ? 'auth url' : 'config file';
  const windows = process.platform === 'win32';
  const command = windows
    ? (isUrl ? 'rundll32.exe' : 'notepad.exe')
    : (process.platform === 'darwin' ? 'open' : 'xdg-open');
  const args = windows && isUrl ? ['url.dll,FileProtocolHandler', target] : [target];
  try {
    const child = spawn(command, args, {
      detached: true,
      stdio: 'ignore',
      windowsHide: isUrl
    });
    child.on('error', (error) => log(`failed to open ${label}: ${error.message}`));
    child.unref();
    return true;
  } catch (error) {
    log(`failed to open ${label}: ${error.message}`);
    return false;
  }
}

function openExternalUrl(url) {
  return openDesktopTarget(url, true);
}

function openTextFile(filePath) {
  return openDesktopTarget(filePath);
}

function createDefaultConfig(configPath) {
    const defaultConfigFile = {
        username: "",
        auth: "microsoft",
        flow: "live",
        host: "mc.hypixel.net",
        fallbackHost: "chi.free.overlag.link",
        partyOwnerUsername: "",
        minecraftPort: 25565,
        version: "1.8.9",
        chatDelayMs: 900,
        partyAcceptDelayMs: 250,
        maxChatQueue: 32
    };

    fs.writeFileSync(configPath, JSON.stringify(defaultConfigFile, null, 2), 'utf8');
    log(`created default config at: ${configPath}`);
}

function loadConfig() {
    const configPath = path.join(__dirname, 'mutedVoiceBot.config.json');
    let config = { ...DEFAULT_CONFIG };

    log(`looking for config at: ${configPath}`);

    if (!fs.existsSync(configPath)) {
        try {
            createDefaultConfig(configPath);
            openTextFile(configPath);
            log('config did not exist, created it and opened it for editing');
        } catch (error) {
            log(`failed to create config: ${error.message}`);
        }
    }

    if (fs.existsSync(configPath)) {
        try {
            const parsed = JSON.parse(fs.readFileSync(configPath, 'utf8'));
            config = { ...config, ...parsed };
            log(`config loaded. username present: ${config.username ? 'yes' : 'no'}`);
            log(`config flow: ${config.flow || 'missing'}`);
        } catch (error) {
            log(`config parse error: ${error.message}`);
        }
    }

    if (config.authTitle === 'Muted Voice' || !config.authTitle) {
        delete config.authTitle;
    }

    if (!Number.isFinite(config.chatDelayMs) || config.chatDelayMs < 0) config.chatDelayMs = DEFAULT_CONFIG.chatDelayMs;
    if (!Number.isFinite(config.partyAcceptDelayMs) || config.partyAcceptDelayMs < 0) config.partyAcceptDelayMs = DEFAULT_CONFIG.partyAcceptDelayMs;
    if (!Number.isInteger(config.maxChatQueue) || config.maxChatQueue < 1) config.maxChatQueue = DEFAULT_CONFIG.maxChatQueue;
    config.host = String(config.host || DEFAULT_CONFIG.host).trim() || DEFAULT_CONFIG.host;
    config.fallbackHost = String(config.fallbackHost || DEFAULT_CONFIG.fallbackHost).trim();
    config.partyOwnerUsername = normalizeMinecraftUsername(
        config.partyOwnerUsername || config.partyOwner || config.allowedPartyOwner || ''
    );

    return config;
}

let mineflayer = null;
try {
  mineflayer = require('mineflayer');
} catch (error) {
  log(`mineflayer require error: ${error.message}`);
}

let config = loadConfig();
let bot = null;
let botGeneration = 0;
let status = 'offline';
let statusMessage = '';
let stopping = false;
let chatQueue = [];
let chatTimer = null;
let lastChatAt = 0;
let latestAuthPrompt = null;
const clients = new Set();
const recentPrivateMessages = new Map();
const recentPartyInviteAccepts = new Map();
const PARTY_INVITE_DEDUPE_MS = 1500;
const pendingPrivateSends = [];

function send(socket, payload) {
  try {
    socket.write(`${JSON.stringify(payload)}\n`);
  } catch (error) {
    log(`control write error: ${error.message}`);
  }
}

function broadcast(payload) {
  for (const socket of clients) send(socket, payload);
}

function reportStatus(socket) {
  send(socket, { type: 'status', status, message: statusMessage });
  if (latestAuthPrompt) send(socket, { type: 'auth', ...latestAuthPrompt });
}

function setStatus(nextStatus, message = '') {
  if (status === nextStatus && statusMessage === message) return;
  status = nextStatus;
  statusMessage = message;
  log(`status=${status}${message ? ` message=${message}` : ''}`);
  broadcast({ type: 'status', status, message });
  if (status === 'online' || status === 'inParty') scheduleChatPump();
}

function setAuthPrompt(prompt) {
  latestAuthPrompt = {
    code: prompt && prompt.code ? String(prompt.code) : '',
    url: prompt && prompt.url ? String(prompt.url) : '',
    message: prompt && prompt.message ? String(prompt.message) : ''
  };
  broadcast({ type: 'auth', ...latestAuthPrompt });
}

function clearAuthPrompt() {
  if (!latestAuthPrompt) return;
  latestAuthPrompt = null;
  broadcast({ type: 'authClear' });
}

function normalizeMessage(message) {
  if (typeof message !== 'string') return '';
  return message.replace(/\r?\n/g, ' ').trim();
}

function normalizeMinecraftUsername(value) {
  const username = String(value || '').trim().replace(/^@+/, '');
  return /^[A-Za-z0-9_]{1,16}$/.test(username) ? username : '';
}

function scheduleChatPump() {
  if (chatTimer || chatQueue.length === 0) return;
  if (!bot) return;
  if (status !== 'online' && status !== 'inParty') return;

  const now = Date.now();
  const waitMs = Math.max(0, config.chatDelayMs - (now - lastChatAt));
  chatTimer = setTimeout(() => {
    chatTimer = null;
    if (!bot || chatQueue.length === 0) return;

    const item = chatQueue.shift();
    try {
      bot.chat(item.message);
      rememberPendingPrivateSend(item.message);
      lastChatAt = Date.now();
      log(`sent chat length=${item.message.length}`);
    } catch (error) {
      setStatus('error', `chat send failed: ${error.message}`);
    }
    scheduleChatPump();
  }, itemDelay(chatQueue[0], waitMs));
}

function itemDelay(item, fallback) {
  if (item && Number.isFinite(item.delayMs)) return Math.max(item.delayMs, fallback);
  return fallback;
}

function queueChat(message, delayMs = null) {
  const normalized = normalizeMessage(message);
  if (!normalized) return false;
  if (!bot) return false;
  if (chatQueue.length >= config.maxChatQueue) {
    log('chat queue full; dropping message');
    return false;
  }

  chatQueue.push({ message: normalized, delayMs });
  scheduleChatPump();
  return true;
}

function privateSendFromCommand(message) {
  const normalized = normalizeMessage(message);
  const match = normalized.match(/^\/msg\s+([A-Za-z0-9_]{1,16})\s+(.+)$/i);
  if (!match) return null;

  return {
    player: match[1],
    message: match[2].trim(),
    sentAt: Date.now()
  };
}

function rememberPendingPrivateSend(message) {
  const pending = privateSendFromCommand(message);
  if (!pending || !pending.message) return;

  const now = Date.now();
  for (let i = pendingPrivateSends.length - 1; i >= 0; i -= 1) {
    if (now - pendingPrivateSends[i].sentAt > 15000) pendingPrivateSends.splice(i, 1);
  }

  pendingPrivateSends.push(pending);
  if (pendingPrivateSends.length > 16) pendingPrivateSends.shift();
}

function clearPendingPrivateSend(player, message) {
  const playerLower = String(player || '').toLowerCase();
  for (let i = 0; i < pendingPrivateSends.length; i += 1) {
    const pending = pendingPrivateSends[i];
    if (pending.player.toLowerCase() !== playerLower) continue;
    if (message && pending.message !== message) continue;
    pendingPrivateSends.splice(i, 1);
    return true;
  }
  return false;
}

function findRunCommand(node) {
  if (!node || typeof node !== 'object') return '';
  const clickEvent = node.clickEvent;
  if (clickEvent && clickEvent.action === 'run_command' && typeof clickEvent.value === 'string') {
    const command = clickEvent.value.trim();
    if (/^\/(?:party|p)\s+accept\b/i.test(command)) return command;
  }

  for (const key of ['extra', 'with']) {
    if (Array.isArray(node[key])) {
      for (const child of node[key]) {
        const command = findRunCommand(child);
        if (command) return command;
      }
    }
  }

  return '';
}

function inviterFromPartyAcceptCommand(command) {
  const match = String(command || '').trim().match(/^\/(?:party|p)\s+accept(?:\s+([A-Za-z0-9_]{1,16}))?\b/i);
  return match && match[1] ? match[1] : '';
}

function textFromMessage(jsonMsg) {
  try {
    return typeof jsonMsg.toString === 'function' ? jsonMsg.toString() : JSON.stringify(jsonMsg);
  } catch (_) {
    return '';
  }
}

function partyInviteFromMessage(jsonMsg) {
  const raw = jsonMsg && (jsonMsg.json || jsonMsg);
  const clickedCommand = findRunCommand(raw);

  const text = textFromMessage(jsonMsg);
  const inviteTextMatch = text.match(/\b([A-Za-z0-9_]{1,16})\b\s+has invited you to join (?:their )?party/i);
  const textInviter = inviteTextMatch ? inviteTextMatch[1] : '';

  if (clickedCommand) {
    return {
      command: clickedCommand,
      inviter: inviterFromPartyAcceptCommand(clickedCommand) || textInviter
    };
  }

  if (!textInviter) return null;
  return {
    command: `/party accept ${textInviter}`,
    inviter: textInviter
  };
}

function allowedPartyOwnerMatches(inviter) {
  const allowedOwner = normalizeMinecraftUsername(config.partyOwnerUsername);
  if (!allowedOwner) return true;
  return normalizeMinecraftUsername(inviter).toLowerCase() === allowedOwner.toLowerCase();
}

function shouldAcceptPartyInvite(invite) {
  if (!invite || !invite.command) return false;
  if (!allowedPartyOwnerMatches(invite.inviter)) {
    log(`ignored party invite from ${invite.inviter || 'unknown'}; allowed owner=${config.partyOwnerUsername}`);
    return false;
  }

  const now = Date.now();
  for (const [key, acceptedAt] of recentPartyInviteAccepts) {
    if (now - acceptedAt > PARTY_INVITE_DEDUPE_MS) recentPartyInviteAccepts.delete(key);
  }

  const dedupeKey = `${normalizeMinecraftUsername(invite.inviter).toLowerCase()}|${invite.command.toLowerCase()}`;
  if (recentPartyInviteAccepts.has(dedupeKey) && now - recentPartyInviteAccepts.get(dedupeKey) <= PARTY_INVITE_DEDUPE_MS) {
    return false;
  }

  recentPartyInviteAccepts.set(dedupeKey, now);
  return true;
}

function acceptPartyInvite(jsonMsg) {
  if (!bot) return;

  const invite = partyInviteFromMessage(jsonMsg);
  if (!shouldAcceptPartyInvite(invite)) return;

  queueChat(invite.command, config.partyAcceptDelayMs);
  setStatus('inParty', `accepted party invite${invite.inviter ? ` from ${invite.inviter}` : ''}`);
  log(`accepted party invite from ${invite.inviter || 'unknown'} using ${invite.command}`);
}

function stripMinecraftFormatting(text) {
  return String(text || '').replace(/\u00a7[0-9A-FK-OR]/gi, '');
}

function privateMessageFromText(text) {
  const clean = stripMinecraftFormatting(text).replace(/\s+/g, ' ').trim();
  const match = clean.match(/^(From|To)\s+(?:(\[[^\]]+\])\s+)?([A-Za-z0-9_]{1,16}):\s*(.+)$/i);
  if (!match) return null;

  return {
    direction: match[1].toLowerCase(),
    rank: match[2] || '',
    player: match[3],
    message: match[4].trim()
  };
}

function privateMessageFailureFromText(text) {
  if (pendingPrivateSends.length === 0) return null;

  const now = Date.now();
  for (let i = pendingPrivateSends.length - 1; i >= 0; i -= 1) {
    if (now - pendingPrivateSends[i].sentAt > 15000) pendingPrivateSends.splice(i, 1);
  }
  if (pendingPrivateSends.length === 0) return null;

  const clean = stripMinecraftFormatting(text).replace(/\s+/g, ' ').trim();
  if (!clean) return null;

  const lower = clean.toLowerCase();
  const failed = [
    /cannot message/,
    /can't message/,
    /could not message/,
    /not accepting/,
    /private messages? (?:are )?disabled/,
    /only message (?:people|players|staff)/,
    /not friends/,
    /must be friends/,
    /player .* not found/,
    /couldn't find/,
    /can't find/,
    /not online/,
    /does not exist/,
    /doesn't exist/
  ].some((pattern) => pattern.test(lower));
  if (!failed) return null;

  let pendingIndex = 0;
  for (let i = 0; i < pendingPrivateSends.length; i += 1) {
    if (lower.includes(pendingPrivateSends[i].player.toLowerCase())) {
      pendingIndex = i;
      break;
    }
  }

  const pending = pendingPrivateSends.splice(pendingIndex, 1)[0];
  return {
    player: pending.player,
    message: clean
  };
}

function rememberPrivateMessage(payload) {
  const now = Date.now();
  for (const [key, timestamp] of recentPrivateMessages) {
    if (now - timestamp > 1500) recentPrivateMessages.delete(key);
  }

  const key = `${payload.rank}|${payload.player.toLowerCase()}|${payload.message}`;
  if (recentPrivateMessages.has(key)) return false;
  recentPrivateMessages.set(key, now);
  return true;
}

function reportPrivateMessage(jsonMsg) {
  const text = textFromMessage(jsonMsg);
  const payload = privateMessageFromText(text);
  if (payload && payload.message) {
    if (payload.direction === 'to' && !clearPendingPrivateSend(payload.player, payload.message)) return;
    if (!rememberPrivateMessage(payload)) return;

    broadcast({
      type: 'privateMessage',
      direction: payload.direction,
      player: payload.player,
      message: payload.message,
      rank: payload.rank
    });
    log(`private message ${payload.direction} player=${payload.player} rankPresent=${payload.rank ? 'yes' : 'no'} length=${payload.message.length}`);
    return;
  }

  const failure = privateMessageFailureFromText(text);
  if (!failure) return;

  broadcast({
    type: 'privateMessage',
    direction: 'failed',
    player: failure.player,
    message: failure.message,
    rank: ''
  });
  log(`private message failed player=${failure.player} reason=${failure.message}`);
}

function resetChatState() {
  chatQueue = [];
  recentPrivateMessages.clear();
  recentPartyInviteAccepts.clear();
  pendingPrivateSends.length = 0;
  if (chatTimer) clearTimeout(chatTimer);
  chatTimer = null;
}

function createBot(useFallback = false) {
  if (!useFallback) config = loadConfig();
  if (!mineflayer) {
    clearAuthPrompt();
    setStatus('error', 'mineflayer is not installed; run npm install mineflayer');
    return;
  }
  if (bot) {
    reportAllStatus();
    return;
  }
    if (!config.username) {
        const configPath = path.join(__dirname, 'mutedVoiceBot.config.json');

        try {
            if (!fs.existsSync(configPath)) {
                createDefaultConfig(configPath);
            }
            openTextFile(configPath);
        } catch (error) {
            log(`failed to open/create config for username: ${error.message}`);
        }

        clearAuthPrompt();
        setStatus('error', 'missing username: fill in mutedVoiceBot.config.json');
        return;
    }

  try {
    fs.mkdirSync(config.profilesFolder, { recursive: true });
  } catch (error) {
    clearAuthPrompt();
    setStatus('error', `auth cache unavailable: ${error.message}`);
    return;
  }

  stopping = false;
  resetChatState();
  const primaryHost = config.host;
  const fallbackHost = config.fallbackHost;
  const targetHost = useFallback ? fallbackHost : primaryHost;
  const canUseFallback = !useFallback && fallbackHost &&
    fallbackHost.toLowerCase() !== primaryHost.toLowerCase();
  const generation = ++botGeneration;
  setStatus('connecting', useFallback ? `using fallback ${targetHost}` : '');
  log(`connecting to ${targetHost}${useFallback ? ' (fallback)' : ' (primary)'}`);

  try {
    bot = mineflayer.createBot({
        host: targetHost,
        port: config.minecraftPort,
        version: config.version,
        username: config.username,
        auth: config.auth,
        flow: config.flow,
        authTitle: config.authTitle,
        profilesFolder: config.profilesFolder,
      onMsaCode: (data) => {
          if (generation !== botGeneration || stopping) return;
          const code = data && (data.user_code || data.userCode || data.code);

          const url = data && (
              data.verification_uri_complete ||
              data.verificationUriComplete ||
              data.verification_uri ||
              data.verificationUri ||
              data.verificationUrl
          );

          const fallbackUrl = 'https://www.microsoft.com/link';
          const authUrl = url || fallbackUrl;

          log(`Microsoft auth prompt received codePresent=${code ? 'yes' : 'no'} urlPresent=${authUrl ? 'yes' : 'no'}`);

          const opened = openExternalUrl(authUrl);
          const message = opened ? 'Microsoft sign in opened' : 'Microsoft sign in required';
          setAuthPrompt({
              code: code || '',
              url: authUrl,
              message
          });
          setStatus('connecting', message);
      }
  });
  } catch (error) {
    bot = null;
    ++botGeneration;
    clearAuthPrompt();
    setStatus('error', `connection failed: ${error.message}`);
    return;
  }

  const currentBot = bot;
  const isCurrent = () => bot === currentBot && generation === botGeneration;
  let loggedIn = false;
  let kicked = false;

  const retryWithFallback = (reason) => {
    if (!isCurrent() || stopping || loggedIn || kicked || !canUseFallback) return false;

    bot = null;
    ++botGeneration;
    resetChatState();
    clearAuthPrompt();
    try {
      currentBot.end();
    } catch (_) {
      // The failed primary connection may already be closed.
    }

    const detail = reason ? `: ${String(reason)}` : '';
    log(`connection to ${targetHost} failed before login${detail}; retrying via ${fallbackHost}`);
    createBot(true);
    return true;
  };

  bot.once('login', () => {
    if (!isCurrent() || stopping) return;
    loggedIn = true;
    clearAuthPrompt();
    setStatus('online');
  });
  bot.once('spawn', () => {
    if (!isCurrent() || stopping) return;
    if (status !== 'inParty') setStatus('online');
    currentBot.clearControlStates();
  });
  bot.on('message', (jsonMsg) => {
    if (!isCurrent() || stopping) return;
    acceptPartyInvite(jsonMsg);
    reportPrivateMessage(jsonMsg);
  });
  bot.on('kicked', (reason) => {
    if (!isCurrent() || stopping) return;
    kicked = true;
    const text = typeof reason === 'string' ? reason : JSON.stringify(reason);
    clearAuthPrompt();
    setStatus('error', `kicked: ${text}`);
    log(`kicked: ${text}`);
  });
  bot.on('error', (error) => {
    if (!isCurrent() || stopping) return;
    log(`bot error host=${targetHost}: ${error.stack || error.message}`);
    if (retryWithFallback(error.message)) return;
    clearAuthPrompt();
    setStatus('error', error.message);
  });
  bot.on('end', (reason) => {
    if (!isCurrent()) return;
    if (retryWithFallback(reason || 'disconnected')) return;
    bot = null;
    ++botGeneration;
    resetChatState();
    clearAuthPrompt();
    if (stopping) setStatus('offline');
    else setStatus('offline', reason ? String(reason) : 'disconnected');
  });
}

function reportAllStatus() {
  broadcast({ type: 'status', status, message: statusMessage });
  if (latestAuthPrompt) broadcast({ type: 'auth', ...latestAuthPrompt });
}

function stopBot(force = false) {
  stopping = true;
  resetChatState();
  clearAuthPrompt();

  const currentBot = bot;
  if (!currentBot) {
    setStatus('offline');
    return;
  }

  if (force) {
    bot = null;
    ++botGeneration;
  }
  try {
    currentBot.quit();
  } catch (error) {
    log(`bot quit error: ${error.message}`);
    force = true;
    bot = null;
    ++botGeneration;
  }
  if (force) {
    try {
      currentBot.end();
    } catch (_) {
      // Ignore secondary shutdown failures.
    }
    setStatus('offline');
  }
}


function signOut() {
  stopBot(true);

  try {
    const resolvedProfilesFolder = path.resolve(config.profilesFolder);
    if (path.basename(resolvedProfilesFolder).toLowerCase() !== '.mineflayer-auth') {
      throw new Error('refusing to delete unexpected auth cache path');
    }

    fs.rmSync(resolvedProfilesFolder, { recursive: true, force: true });
    log('auth cache removed for sign out');
    setStatus('offline', 'signed out');
    return { ok: true, message: 'signed out' };
  } catch (error) {
    log(`sign out failed: ${error.message}`);
    setStatus('error', `sign out failed: ${error.message}`);
    return { ok: false, message: `sign out failed: ${error.message}` };
  }
}

function handleCommand(socket, command) {
  switch (command.cmd) {
    case 'start':
      createBot();
      reportStatus(socket);
      break;
    case 'stop':
      stopBot();
      reportStatus(socket);
      break;
    case 'signOut': {
      const result = signOut();
      send(socket, { type: result.ok ? 'ok' : 'error', status, message: result.message });
      break;
    }
    case 'status':
      reportStatus(socket);
      break;
    case 'sendChat':
      if (!queueChat(command.message)) {
        send(socket, { type: 'error', status, message: 'chat not queued' });
      } else {
        send(socket, { type: 'ok', status, message: 'chat queued' });
      }
      break;
    case 'setPartyOwner':
      config.partyOwnerUsername = normalizeMinecraftUsername(
        command.partyOwnerUsername || command.partyOwner || ''
      );
      recentPartyInviteAccepts.clear();
      log(`party owner username updated through control server: ${config.partyOwnerUsername || 'any'}`);
      send(socket, { type: 'ok', status, message: 'party owner updated' });
      break;
    case 'shutdown':
      stopBot();
      send(socket, { type: 'ok', status: 'offline', message: 'shutdown' });
      setTimeout(() => {
        server.close(() => process.exit(0));
        setTimeout(() => process.exit(0), 1000).unref();
      }, 250).unref();
      break;
    default:
      send(socket, { type: 'error', status, message: `unknown command: ${command.cmd || ''}` });
      break;
  }
}

const server = net.createServer((socket) => {
  clients.add(socket);
  socket.setEncoding('utf8');
  reportStatus(socket);

  let buffer = '';
  socket.on('data', (chunk) => {
    buffer += chunk;
    if (buffer.length > 64 * 1024) {
      socket.destroy();
      return;
    }
    let newline = buffer.indexOf('\n');
    while (newline !== -1) {
      const line = buffer.slice(0, newline).trim();
      buffer = buffer.slice(newline + 1);
      if (line) {
        try {
          handleCommand(socket, JSON.parse(line));
        } catch (error) {
          send(socket, { type: 'error', status, message: `bad command: ${error.message}` });
        }
      }
      newline = buffer.indexOf('\n');
    }
  });

  socket.on('error', (error) => log(`control socket error: ${error.message}`));
  socket.on('close', () => clients.delete(socket));
});

server.on('error', (error) => {
    if (error && error.code === 'EADDRINUSE') {
        setStatus('error', `control port ${controlPort} already in use; close the old mutedVoiceBot/node process`);
        log(`control server error: port ${controlPort} already in use. Another mutedVoiceBot.js is probably already running.`);
    } else {
        setStatus('error', error.message);
        log(`control server error: ${error.stack || error.message}`);
    }

    process.exitCode = 1;
    setTimeout(() => process.exit(1), 100).unref();
});

server.listen(controlPort, '127.0.0.1', () => {
  setStatus('offline');
  log(`control server listening on 127.0.0.1:${controlPort}`);
});

process.on('SIGINT', () => {
  stopBot();
  setTimeout(() => process.exit(0), 500).unref();
});

process.on('SIGTERM', () => {
  stopBot();
  setTimeout(() => process.exit(0), 500).unref();
});
