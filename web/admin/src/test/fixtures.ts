// What a master answers in the tests: a server, a game of two players, a player in the lobby, as
// docs/protocol/admin-api.md writes them.

import type { Account, Action, Ban, Game, Player, ServerInfo } from '../api/types';

export const now = 1798000000000;

export const server: ServerInfo = {
  version: '0.4.0',
  startedAt: now - 3_600_000,
  uptime: 3_600_000,
  ports: { lobby: 4242, game: 4242, panel: 8080 },
  maxGames: 57,
  players: 3,
  games: 1,
  bans: 1,
};

export const game: Game = {
  id: 3,
  name: 'Bydo hunters',
  maxPlayers: 4,
  port: 4242,
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

export const accounts: Account[] = [
  {
    id: 3,
    username: 'Alice',
    email: 'alice@example.org',
    emailVerifiedAt: now - 86_400_000,
    password: true,
    discordId: null,
    role: 'player',
    locked: false,
    bannedAt: null,
    banReason: '',
    createdAt: now - 86_400_000,
    lastSeenAt: now - 60_000,
  },
  {
    id: 1,
    username: 'Chief',
    email: 'chief@example.org',
    emailVerifiedAt: now - 172_800_000,
    password: true,
    discordId: '4242',
    discord: { username: 'chief', displayName: 'The Chief', avatar: 'a1b2' },
    role: 'admin',
    locked: true,
    bannedAt: null,
    banReason: '',
    createdAt: now - 172_800_000,
    lastSeenAt: now,
  },
];
