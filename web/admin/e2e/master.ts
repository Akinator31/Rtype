// Starts what the end-to-end tests of the panel need: a real master, r-type_server, with accounts, serving the
// panel built in dist/, and a game of two real players, rtype_panel_players (tests/helpers/panel_players.cpp).
// Its accounts are written before it starts by rtype_panel_accounts (tests/helpers/panel_accounts.cpp): an
// administrator and a player; its mail server is a small one of the tests, which takes every mail. The tests
// read the address of admin-url.txt, the id of the game and what they need to start players of their own from
// the environment.
//
// The programs are looked for in RTYPE_BUILD_BIN, by default build/bin at the root of the repository.

import { spawn, type ChildProcess } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { createServer, type Server } from 'node:net';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const panel = resolve(here, '../dist');
const bin = resolve(process.env.RTYPE_BUILD_BIN ?? resolve(here, '../../../build/bin'));
const executable = (name: string) => join(bin, process.platform === 'win32' ? `${name}.exe` : name);

/** A TCP port free now. */
function freeTcpPort(): Promise<number> {
  return new Promise((done, fail) => {
    const server = createServer();
    server.once('error', fail);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      server.close(() =>
        typeof address === 'object' && address ? done(address.port) : fail(new Error('no port')),
      );
    });
  });
}

function waitFor<T>(what: string, check: () => T | undefined, limit = 20_000): Promise<T> {
  const deadline = Date.now() + limit;
  return new Promise((done, fail) => {
    const poll = () => {
      const found = check();
      if (found !== undefined) return done(found);
      if (Date.now() > deadline) return fail(new Error(`Timed out waiting for ${what}`));
      setTimeout(poll, 50);
    };
    poll();
  });
}

function started(program: ChildProcess, output: { text: string }, name: string): void {
  program.stdout?.on('data', (chunk: Buffer) => (output.text += chunk.toString()));
  program.stderr?.on('data', (chunk: Buffer) => (output.text += chunk.toString()));
  program.on('exit', (code) => {
    if (code !== null && code !== 0) console.error(`${name} exited with ${code}:\n${output.text}`);
  });
}

/** The accounts written before the master starts; the administrator is in [admin] admins too. */
export const AdminLogin = 'admin@example.org';
export const PlayerName = 'Dana';
export const AccountPassword = 'correct horse battery';

/**
 * A mail server of the tests, in the clear: it says yes to everything, which is all the master checks at start
 * and all its mails need.
 */
function mailServer(): Promise<{ server: Server; port: number }> {
  return new Promise((done, fail) => {
    const server = createServer((socket) => {
      let data = false;
      socket.setEncoding('utf8');
      socket.write('220 mail.test ESMTP\r\n');
      let buffer = '';
      socket.on('data', (chunk: string) => {
        buffer += chunk;
        let end = buffer.indexOf('\r\n');
        while (end !== -1) {
          const line = buffer.slice(0, end);
          buffer = buffer.slice(end + 2);
          if (data) {
            if (line === '.') {
              data = false;
              socket.write('250 queued\r\n');
            }
          } else if (/^DATA/i.test(line)) {
            data = true;
            socket.write('354 go on\r\n');
          } else if (/^QUIT/i.test(line)) {
            socket.end('221 bye\r\n');
          } else if (/^EHLO/i.test(line)) {
            socket.write('250-mail.test\r\n250 8BITMIME\r\n');
          } else {
            socket.write('250 ok\r\n');
          }
          end = buffer.indexOf('\r\n');
        }
      });
      socket.on('error', () => undefined);
    });
    server.once('error', fail);
    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      return typeof address === 'object' && address
        ? done({ server, port: address.port })
        : fail(new Error('no port'));
    });
  });
}

function run(program: string, args: string[], cwd: string): Promise<void> {
  return new Promise((done, fail) => {
    const output = { text: '' };
    const child = spawn(program, args, { cwd, stdio: ['ignore', 'pipe', 'pipe'] });
    child.stdout.on('data', (chunk: Buffer) => (output.text += chunk.toString()));
    child.stderr.on('data', (chunk: Buffer) => (output.text += chunk.toString()));
    child.on('exit', (code) =>
      code === 0 ? done() : fail(new Error(`${program} exited with ${code}:\n${output.text}`)),
    );
  });
}

export default async function setup(): Promise<() => Promise<void>> {
  for (const needed of [
    join(panel, 'index.html'),
    executable('r-type_server'),
    executable('rtype_panel_players'),
    executable('rtype_panel_accounts'),
  ]) {
    if (!existsSync(needed)) {
      throw new Error(`${needed} is missing: build the panel (npm run build) and the server with its tests`);
    }
  }
  const folder = mkdtempSync(join(tmpdir(), 'rtype-panel-e2e-'));
  const lobbyPort = await freeTcpPort();
  const mail = await mailServer();
  await run(
    executable('rtype_panel_accounts'),
    [
      'sqlite:rtype.db',
      `Admin:${AdminLogin}:${AccountPassword}`,
      `${PlayerName}:dana@example.org:${AccountPassword}`,
    ],
    folder,
  );
  writeFileSync(
    join(folder, 'server.toml'),
    [
      '[server]',
      `port = ${lobbyPort}`,
      'game_port = 0',
      'max_games = 4',
      'key = "server.key"',
      'database = "sqlite:rtype.db"',
      '',
      '[web]',
      'listen = "127.0.0.1:0"',
      `panel_dir = ${JSON.stringify(panel)}`,
      '',
      '[accounts]',
      'guests = true',
      '',
      '[mail]',
      'host = "127.0.0.1"',
      `port = ${mail.port}`,
      'security = "none"',
      'from = "R-Type <rtype@example.org>"',
      '',
      '[admin]',
      `admins = [${JSON.stringify(AdminLogin)}]`,
      '',
    ].join('\n'),
  );

  const masterOutput = { text: '' };
  const master = spawn(executable('r-type_server'), ['--settings', 'server.toml', '--log-level', 'warning'], {
    cwd: folder,
    stdio: ['ignore', 'pipe', 'pipe'],
  });
  started(master, masterOutput, 'r-type_server');
  const url = await waitFor('admin-url.txt', () => {
    const file = join(folder, 'admin-url.txt');
    const text = existsSync(file) ? readFileSync(file, 'utf8').trim() : '';
    return text.includes('?token=') ? text : undefined;
  });

  const playersOutput = { text: '' };
  const players = spawn(
    executable('rtype_panel_players'),
    [String(lobbyPort), join(folder, 'server.key'), 'Playwright', 'Alice', 'Bob'],
    { stdio: ['ignore', 'pipe', 'pipe'] },
  );
  started(players, playersOutput, 'rtype_panel_players');
  const gameId = await waitFor('the game of the players', () => /ready (\d+)/.exec(playersOutput.text)?.[1]);

  process.env.RTYPE_PANEL_URL = url;
  process.env.RTYPE_PANEL_GAME = gameId;
  // For the tests that start players of their own
  process.env.RTYPE_PANEL_PLAYERS = executable('rtype_panel_players');
  process.env.RTYPE_PANEL_LOBBY_PORT = String(lobbyPort);
  process.env.RTYPE_PANEL_KEY = join(folder, 'server.key');

  return async () => {
    players.kill();
    master.kill('SIGTERM');
    await new Promise((done) => (master.exitCode !== null ? done(null) : master.once('exit', done)));
    mail.server.close();
    rmSync(folder, { recursive: true, force: true });
  };
}
