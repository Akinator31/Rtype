// Starts what the end-to-end tests of the panel need: a real master, r-type_server, serving the panel built in
// dist/, and a game of two real players, rtype_panel_players (tests/helpers/panel_players.cpp). The tests read
// the address of admin-url.txt and the id of the game from the environment.
//
// The programs are looked for in RTYPE_BUILD_BIN, by default build/bin at the root of the repository.

import { spawn, type ChildProcess } from 'node:child_process';
import { createSocket } from 'node:dgram';
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { createServer } from 'node:net';
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

function udpPortFree(port: number): Promise<boolean> {
  return new Promise((done) => {
    const socket = createSocket('udp4');
    socket.once('error', () => done(false));
    socket.bind(port, () => socket.close(() => done(true)));
  });
}

/** `count` consecutive UDP ports free now, for the games of the master. */
async function freeUdpPorts(count: number): Promise<number> {
  for (let attempt = 0; attempt < 100; attempt++) {
    const first = 20000 + Math.floor(Math.random() * 40000);
    let free = true;
    for (let port = first; free && port < first + count; port++) free = await udpPortFree(port);
    if (free) return first;
  }
  throw new Error('No free UDP ports');
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

export default async function setup(): Promise<() => Promise<void>> {
  for (const needed of [
    join(panel, 'index.html'),
    executable('r-type_server'),
    executable('rtype_panel_players'),
  ]) {
    if (!existsSync(needed)) {
      throw new Error(`${needed} is missing: build the panel (npm run build) and the server with its tests`);
    }
  }
  const folder = mkdtempSync(join(tmpdir(), 'rtype-panel-e2e-'));
  const lobbyPort = await freeTcpPort();
  const firstGamePort = await freeUdpPorts(4);
  writeFileSync(
    join(folder, 'server.toml'),
    [
      '[server]',
      `port = ${lobbyPort}`,
      `game_ports = "${firstGamePort}-${firstGamePort + 3}"`,
      'key = "server.key"',
      'database = "sqlite:rtype.db"',
      '',
      '[web]',
      'listen = "127.0.0.1:0"',
      `panel_dir = ${JSON.stringify(panel)}`,
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

  return async () => {
    players.kill();
    master.kill('SIGTERM');
    await new Promise((done) => (master.exitCode !== null ? done(null) : master.once('exit', done)));
    rmSync(folder, { recursive: true, force: true });
  };
}
