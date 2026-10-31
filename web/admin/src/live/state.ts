// What the panel knows of the server, live: the snapshot of /api/events, then each event applied to it
// (docs/protocol/admin-api.md, 6). Pure: the store feeds it the frames of its WebSocket.

import type { Action, Game, LiveEvent, Player, ServerInfo } from '../api/types';

export type Connection = 'connecting' | 'open' | 'reconnecting' | 'signed-out';

/** The tone of a line of the feed, after the notifications of the design: information, bonus, alert. */
export type Tone = 'info' | 'bonus' | 'alert';

/** One line of the feed of events: a subject lit, what happened, and an object lit. */
export interface FeedEntry {
  key: number;
  at: number;
  tone: Tone;
  subject: string;
  verb: string;
  object?: string;
  /** The game it happened in, for the feed of a game. */
  gameId?: number;
}

/** The number of games and players at a moment, for the chart of the overview. */
export interface Sample {
  at: number;
  games: number;
  players: number;
}

export interface LiveState {
  connection: Connection;
  /** The attempts since the WebSocket closed, and when the next one starts: while reconnecting. */
  attempt: number;
  retryAt: number | null;
  /** When the last snapshot, and the last frame, came; the events since the snapshot. */
  snapshotAt: number | null;
  lastFrameAt: number | null;
  events: number;
  server: ServerInfo | null;
  games: Game[];
  players: Player[];
  /** The latest events, newest first. */
  feed: FeedEntry[];
  /** The actions recorded since the panel opened, newest first. */
  actions: Action[];
  /** Games and players over time, oldest first, since the panel opened. */
  history: Sample[];
}

export const FeedLength = 40;
export const HistoryLength = 720;
/** Samples closer than this are one: the chart keeps a point per few seconds. */
export const SampleInterval = 5000;

export const initialState: LiveState = {
  connection: 'connecting',
  attempt: 0,
  retryAt: null,
  snapshotAt: null,
  lastFrameAt: null,
  events: 0,
  server: null,
  games: [],
  players: [],
  feed: [],
  actions: [],
  history: [],
};

let feedKey = 0;

function upsert<T extends { id: number }>(list: T[], item: T): T[] {
  const index = list.findIndex((existing) => existing.id === item.id);
  if (index === -1) {
    return [...list, item].sort((a, b) => a.id - b.id);
  }
  const copy = list.slice();
  copy[index] = item;
  return copy;
}

function withFeed(state: LiveState, entry: Omit<FeedEntry, 'key'>): FeedEntry[] {
  return [{ ...entry, key: ++feedKey }, ...state.feed].slice(0, FeedLength);
}

function withSample(state: LiveState, at: number): Sample[] {
  const sample = { at, games: state.games.length, players: state.players.length };
  const last = state.history[state.history.length - 1];
  if (last !== undefined && at - last.at < SampleInterval) {
    return [...state.history.slice(0, -1), { ...sample, at: last.at }];
  }
  return [...state.history, sample].slice(-HistoryLength);
}

const endings: Record<string, string> = {
  empty: 'ended, empty',
  over: 'ended, game over',
  crashed: 'crashed',
  ended_by_admin: 'ended by an administrator',
  master_stopping: 'ended, the server stops',
};

/** The game an action was about: its target, or the game of the player it removed. */
export function gameOf(action: Action): number | undefined {
  const target = /^game:(\d+)$/.exec(action.target);
  if (target?.[1] !== undefined) return Number(target[1]);
  return typeof action.details.gameId === 'number' ? action.details.gameId : undefined;
}

/** The words of an action of the record: who did what to whom. */
export function describeAction(action: Action): Omit<FeedEntry, 'key' | 'at'> {
  const details = action.details;
  const name = typeof details.name === 'string' ? details.name : undefined;
  const reason = typeof details.reason === 'string' && details.reason !== '' ? `: ${details.reason}` : '';
  const gameId = gameOf(action);
  switch (action.action) {
    case 'kick':
      return {
        tone: 'alert',
        subject: name ?? action.target,
        verb: `removed${reason}`,
        gameId,
      };
    case 'end_game':
      return {
        tone: 'alert',
        subject: name ?? action.target,
        verb: `ended${reason}`,
        gameId,
      };
    case 'message': {
      const recipients = typeof details.recipients === 'number' ? details.recipients : 0;
      const target = action.target === 'everyone' ? 'everyone' : action.target.replace('game:', 'game ');
      return {
        tone: 'bonus',
        subject: 'Message',
        verb: `to ${target}, ${recipients} received it`,
        gameId,
      };
    }
    case 'ban':
      return {
        tone: 'alert',
        subject: action.target.replace('address:', ''),
        verb: `banned${reason}`,
      };
    case 'unban':
      return {
        tone: 'bonus',
        subject: action.target.replace('address:', ''),
        verb: `ban lifted${reason}`,
      };
    case 'ban_account':
      return { tone: 'alert', subject: name ?? action.target, verb: `banned${reason}` };
    case 'unban_account':
      return { tone: 'bonus', subject: name ?? action.target, verb: `unbanned${reason}` };
    case 'role':
      return {
        tone: 'info',
        subject: name ?? action.target,
        verb: details.role === 'admin' ? 'made administrator' : 'made player',
      };
    case 'delete_account':
      return {
        tone: 'alert',
        subject: action.target.replace('account:', 'account '),
        verb: `deleted${reason}`,
      };
    default:
      return { tone: 'info', subject: action.action, verb: action.target };
  }
}

/** The state after a frame of the WebSocket, received at `now`. */
export function applyEvent(state: LiveState, event: LiveEvent, now: number): LiveState {
  let next: LiveState;
  switch (event.type) {
    case 'snapshot':
      next = {
        ...state,
        server: event.server,
        games: [...event.games].sort((a, b) => a.id - b.id),
        players: [...event.players].sort((a, b) => a.id - b.id),
        snapshotAt: now,
        events: -1,
      };
      break;
    case 'player.joined':
      next = {
        ...state,
        players: upsert(state.players, event.player),
        feed: withFeed(state, {
          at: now,
          tone: 'info',
          subject: event.player.name,
          verb: 'joined the lobby',
        }),
      };
      break;
    case 'player.updated': {
      const before = state.players.find((player) => player.id === event.player.id);
      const gameId = event.player.gameId;
      const entered = gameId !== null && before?.gameId !== gameId;
      const game = state.games.find((candidate) => candidate.id === gameId);
      next = {
        ...state,
        players: upsert(state.players, event.player),
        feed: entered
          ? withFeed(state, {
              at: now,
              tone: 'bonus',
              subject: event.player.name,
              verb: 'joined',
              object: game?.name ?? `game ${gameId}`,
              gameId,
            })
          : state.feed,
      };
      break;
    }
    case 'player.left': {
      const left = state.players.find((player) => player.id === event.playerId);
      next = {
        ...state,
        players: state.players.filter((player) => player.id !== event.playerId),
        feed: left
          ? withFeed(state, {
              at: now,
              tone: 'info',
              subject: left.name,
              verb: 'left the server',
              gameId: left.gameId ?? undefined,
            })
          : state.feed,
      };
      break;
    }
    case 'game.started':
      next = {
        ...state,
        games: upsert(state.games, event.game),
        feed: withFeed(state, {
          at: now,
          tone: 'bonus',
          subject: event.game.name,
          verb: 'started',
          gameId: event.game.id,
        }),
      };
      break;
    case 'game.updated': {
      const before = state.games.find((game) => game.id === event.game.id);
      // Stuck, or its wave reached: worth a line; the status of every second is not
      const stuck = event.game.stale && before?.stale === false;
      const waved = event.game.wave !== null && before !== undefined && before.wave !== event.game.wave;
      let feed = state.feed;
      if (stuck) {
        feed = withFeed(state, {
          at: now,
          tone: 'alert',
          subject: event.game.name,
          verb: 'no status for 3 s',
          gameId: event.game.id,
        });
      } else if (waved) {
        feed = withFeed(state, {
          at: now,
          tone: 'info',
          subject: event.game.name,
          verb: `wave ${event.game.wave} reached`,
          gameId: event.game.id,
        });
      }
      next = { ...state, games: upsert(state.games, event.game), feed };
      break;
    }
    case 'game.ended': {
      const ended = state.games.find((game) => game.id === event.gameId);
      next = {
        ...state,
        games: state.games.filter((game) => game.id !== event.gameId),
        feed: withFeed(state, {
          at: now,
          tone: event.reason === 'crashed' || event.reason === 'master_stopping' ? 'alert' : 'info',
          subject: ended?.name ?? `Game ${event.gameId}`,
          verb: endings[event.reason] ?? `ended, ${event.reason}`,
          gameId: event.gameId,
        }),
      };
      break;
    }
    case 'action':
      next = {
        ...state,
        actions: [event.action, ...state.actions.filter((action) => action.id !== event.action.id)],
        feed: withFeed(state, { at: now, ...describeAction(event.action) }),
      };
      break;
    default:
      return state;
  }
  return { ...next, history: withSample(next, now), lastFrameAt: now, events: next.events + 1 };
}

/** The counts of the server, from the lists kept live rather than the snapshot. */
export function counts(state: LiveState): { players: number; games: number; playing: number } {
  return {
    players: state.players.length,
    games: state.games.length,
    playing: state.players.filter((player) => player.gameId !== null).length,
  };
}
