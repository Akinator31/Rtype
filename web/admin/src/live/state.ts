// What the panel knows of the server, live: the snapshot of /api/events, then each event applied to it
// (docs/protocol/admin-api.md, 5). Pure: the store feeds it the frames of its WebSocket.

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
}

/** The number of games and players at a moment, for the chart of the overview. */
export interface Sample {
  at: number;
  games: number;
  players: number;
}

export interface LiveState {
  connection: Connection;
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

/** The words of an action of the record: who did what to whom. */
export function describeAction(action: Action): Omit<FeedEntry, 'key' | 'at'> {
  const details = action.details;
  const name = typeof details.name === 'string' ? details.name : undefined;
  const reason = typeof details.reason === 'string' && details.reason !== '' ? `: ${details.reason}` : '';
  switch (action.action) {
    case 'kick':
      return {
        tone: 'alert',
        subject: name ?? action.target,
        verb: `removed${reason}`,
        object: action.actor,
      };
    case 'end_game':
      return { tone: 'alert', subject: name ?? action.target, verb: `ended${reason}`, object: action.actor };
    case 'message': {
      const recipients = typeof details.recipients === 'number' ? details.recipients : 0;
      const target = action.target === 'everyone' ? 'everyone' : action.target.replace('game:', 'game ');
      return {
        tone: 'bonus',
        subject: 'Message',
        verb: `to ${target}, ${recipients} received it, by`,
        object: action.actor,
      };
    }
    case 'ban':
      return {
        tone: 'alert',
        subject: action.target.replace('address:', ''),
        verb: `banned${reason}`,
        object: action.actor,
      };
    case 'unban':
      return {
        tone: 'bonus',
        subject: action.target.replace('address:', ''),
        verb: `ban lifted${reason}`,
        object: action.actor,
      };
    default:
      return { tone: 'info', subject: action.action, verb: action.target, object: action.actor };
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
    case 'player.updated':
      next = { ...state, players: upsert(state.players, event.player) };
      break;
    case 'player.left': {
      const left = state.players.find((player) => player.id === event.playerId);
      next = {
        ...state,
        players: state.players.filter((player) => player.id !== event.playerId),
        feed: left
          ? withFeed(state, { at: now, tone: 'info', subject: left.name, verb: 'left the server' })
          : state.feed,
      };
      break;
    }
    case 'game.started':
      next = {
        ...state,
        games: upsert(state.games, event.game),
        feed: withFeed(state, { at: now, tone: 'bonus', subject: event.game.name, verb: 'started' }),
      };
      break;
    case 'game.updated':
      next = { ...state, games: upsert(state.games, event.game) };
      break;
    case 'game.ended': {
      const ended = state.games.find((game) => game.id === event.gameId);
      next = {
        ...state,
        games: state.games.filter((game) => game.id !== event.gameId),
        feed: withFeed(state, {
          at: now,
          tone: event.reason === 'crashed' ? 'alert' : 'info',
          subject: ended?.name ?? `Game ${event.gameId}`,
          verb: endings[event.reason] ?? `ended, ${event.reason}`,
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
  return { ...next, history: withSample(next, now) };
}

/** The counts of the server, from the lists kept live rather than the snapshot. */
export function counts(state: LiveState): { players: number; games: number; playing: number } {
  return {
    players: state.players.length,
    games: state.games.length,
    playing: state.players.filter((player) => player.gameId !== null).length,
  };
}
