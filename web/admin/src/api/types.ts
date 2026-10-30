// The objects of the administration API, as docs/protocol/admin-api.md specifies them. Times are Unix
// milliseconds, durations milliseconds, except the duration of a ban, in seconds.

export interface ServerInfo {
  version: string;
  startedAt: number;
  uptime: number;
  ports: { lobby: number; games: { first: number; last: number }; panel: number };
  players: number;
  games: number;
  bans: number;
}

export interface GamePlayer {
  id: number;
  name: string;
  /** From the last status of the instance: null until the client of the player connected. */
  score: number | null;
  alive: boolean | null;
  /** In milliseconds, null until measured. */
  latency: number | null;
}

export interface Game {
  id: number;
  name: string;
  maxPlayers: number;
  port: number;
  createdAt: number;
  tick: number | null;
  wave: number | null;
  statusAge: number;
  stale: boolean;
  players: GamePlayer[];
}

export interface Player {
  id: number;
  name: string;
  address: string;
  gameId: number | null;
  joinedAt: number;
  /** The account signed in, null for a guest. */
  accountId?: number | null;
}

export type Role = 'player' | 'admin';

/** An account, as GET /api/accounts lists it. */
export interface Account {
  id: number;
  username: string;
  email: string | null;
  emailVerifiedAt: number | null;
  /** Whether it signs in with a password. */
  password: boolean;
  discordId: string | null;
  role: Role;
  /** One of [admin] admins of the settings: never demoted, banned nor deleted from the panel. */
  locked: boolean;
  bannedAt: number | null;
  banReason: string;
  createdAt: number;
  lastSeenAt: number;
}

/** The administrator signed in, null for the local token. */
export interface SessionAccount {
  id: number;
  username: string;
  role: Role;
  locked: boolean;
}

export interface SessionInfo {
  csrfToken: string;
  account: SessionAccount | null;
}

/** How the panel can be opened, besides the local token. */
export interface SignInOptions {
  password: boolean;
  discord: boolean;
}

export type ActionKind =
  | 'kick'
  | 'end_game'
  | 'message'
  | 'ban'
  | 'unban'
  | 'ban_account'
  | 'unban_account'
  | 'role'
  | 'delete_account';

export interface Action {
  id: number;
  at: number;
  actor: string;
  action: ActionKind | string;
  target: string;
  details: Record<string, unknown>;
}

export interface Ban {
  address: string;
  createdAt: number;
  expiresAt: number;
  reason: string;
  actor: string;
}

export type GameEndReason = 'empty' | 'over' | 'crashed' | 'ended_by_admin' | 'master_stopping';

/** A frame of the WebSocket of /api/events. */
export type LiveEvent =
  | { type: 'snapshot'; server: ServerInfo; games: Game[]; players: Player[] }
  | { type: 'player.joined'; player: Player }
  | { type: 'player.updated'; player: Player }
  | { type: 'player.left'; playerId: number }
  | { type: 'game.started'; game: Game }
  | { type: 'game.updated'; game: Game }
  | { type: 'game.ended'; gameId: number; reason: GameEndReason | string }
  | { type: 'action'; action: Action };
