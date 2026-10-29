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
}

export type ActionKind = 'kick' | 'end_game' | 'message' | 'ban';

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
