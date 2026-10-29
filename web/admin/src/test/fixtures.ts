// What a master answers in the tests: a server, a game of two players, a player in the lobby, as
// docs/protocol/admin-api.md writes them.

import type { Action, Ban, Game, Player, ServerInfo } from '../api/types';

export const now = 1798000000000;

export const server: ServerInfo = {
  version: '0.4.0',
  startedAt: now - 3_600_000,
  uptime: 3_600_000,
  ports: { lobby: 4242, games: { first: 4243, last: 4299 }, panel: 8080 },
  players: 3,
  games: 1,
  bans: 1,
};

export const game: Game = {
  id: 3,
  name: 'Bydo hunters',
  maxPlayers: 4,
  port: 4243,
  createdAt: now - 60_000,
  tick: 5400,
  wave: 2,
  statusAge: 412,
  stale: false,
  players: [
    { id: 7, name: 'Alice', score: 1500, alive: true, latency: 35 },
    { id: 9, name: 'Bob', score: null, alive: null, latency: null },
  ],
};

export const players: Player[] = [
  { id: 7, name: 'Alice', address: '203.0.113.9', gameId: 3, joinedAt: now - 120_000 },
  { id: 8, name: 'Carol', address: '198.51.100.4', gameId: null, joinedAt: now - 30_000 },
  { id: 9, name: 'Bob', address: '203.0.113.10', gameId: 3, joinedAt: now - 90_000 },
];

export const kick: Action = {
  id: 12,
  at: now - 1000,
  actor: 'local',
  action: 'kick',
  target: 'player:7',
  details: { name: 'Alice', address: '203.0.113.9', gameId: 3, reason: 'cheating' },
};

export const ban: Ban = {
  address: '198.51.100.4',
  createdAt: now,
  expiresAt: now + 3_600_000,
  reason: 'flood',
  actor: 'local',
};

export const csrfToken = 'c'.repeat(64);
