import { describe, expect, it } from 'vitest';

import type { LiveEvent } from '../api/types';
import * as fixtures from '../test/fixtures';
import {
  FeedLength,
  SampleInterval,
  applyEvent,
  counts,
  describeAction,
  initialState,
  type LiveState,
} from './state';

const snapshot: LiveEvent = {
  type: 'snapshot',
  server: fixtures.server,
  games: [fixtures.game],
  players: [...fixtures.players].reverse(),
};

function apply(events: LiveEvent[], start = initialState, at = fixtures.now): LiveState {
  return events.reduce((state, event, index) => applyEvent(state, event, at + index), start);
}

describe('the live state', () => {
  it('starts from the snapshot, its lists sorted by id', () => {
    const state = apply([snapshot]);
    expect(state.server).toEqual(fixtures.server);
    expect(state.players.map((player) => player.id)).toEqual([7, 8, 9]);
    expect(state.games).toEqual([fixtures.game]);
    expect(counts(state)).toEqual({ players: 3, games: 1, playing: 2 });
  });

  it('follows the players: joined, moved to a game, gone', () => {
    const dave = { id: 10, name: 'Dave', address: '192.0.2.1', gameId: null, joinedAt: fixtures.now };
    let state = apply([snapshot, { type: 'player.joined', player: dave }]);
    expect(state.players.map((player) => player.id)).toEqual([7, 8, 9, 10]);
    expect(state.feed[0]).toMatchObject({ tone: 'info', subject: 'Dave', verb: 'joined the lobby' });

    state = apply([{ type: 'player.updated', player: { ...dave, gameId: 3 } }], state);
    expect(state.players.find((player) => player.id === 10)?.gameId).toBe(3);
    // Moving into a game is not news for the feed
    expect(state.feed).toHaveLength(1);

    state = apply([{ type: 'player.left', playerId: 10 }], state);
    expect(state.players.map((player) => player.id)).toEqual([7, 8, 9]);
    expect(state.feed[0]).toMatchObject({ subject: 'Dave', verb: 'left the server' });
  });

  it('follows the games: started, updated every second, ended with a reason', () => {
    const second = { ...fixtures.game, id: 4, name: 'Duo', players: [] };
    let state = apply([snapshot, { type: 'game.started', game: second }]);
    expect(state.games.map((game) => game.id)).toEqual([3, 4]);
    expect(state.feed[0]).toMatchObject({ tone: 'bonus', subject: 'Duo', verb: 'started' });

    state = apply([{ type: 'game.updated', game: { ...second, wave: 5 } }], state);
    expect(state.games[1]?.wave).toBe(5);
    expect(state.feed).toHaveLength(1);

    state = apply([{ type: 'game.ended', gameId: 4, reason: 'crashed' }], state);
    expect(state.games.map((game) => game.id)).toEqual([3]);
    expect(state.feed[0]).toMatchObject({ tone: 'alert', subject: 'Duo', verb: 'crashed' });

    state = apply([{ type: 'game.ended', gameId: 3, reason: 'ended_by_admin' }], state);
    expect(state.games).toEqual([]);
    expect(state.feed[0]).toMatchObject({
      tone: 'info',
      subject: 'Bydo hunters',
      verb: 'ended by an administrator',
    });
  });

  it('keeps the actions recorded, newest first, each once', () => {
    const later = { ...fixtures.kick, id: 13 };
    const state = apply([
      snapshot,
      { type: 'action', action: fixtures.kick },
      { type: 'action', action: later },
      { type: 'action', action: fixtures.kick },
    ]);
    expect(state.actions.map((action) => action.id)).toEqual([12, 13]);
  });

  it('keeps the feed short', () => {
    const joins: LiveEvent[] = Array.from({ length: FeedLength + 10 }, (_, index) => ({
      type: 'player.joined',
      player: { id: 100 + index, name: `P${index}`, address: '192.0.2.1', gameId: null, joinedAt: 0 },
    }));
    const state = apply(joins);
    expect(state.feed).toHaveLength(FeedLength);
    expect(state.feed[0]?.subject).toBe(`P${FeedLength + 9}`);
  });

  it('samples the games and the players over time, one point per few seconds', () => {
    let state = applyEvent(initialState, snapshot, fixtures.now);
    state = applyEvent(state, { type: 'player.left', playerId: 8 }, fixtures.now + 100);
    expect(state.history).toEqual([{ at: fixtures.now, games: 1, players: 2 }]);
    state = applyEvent(
      state,
      { type: 'game.ended', gameId: 3, reason: 'empty' },
      fixtures.now + SampleInterval,
    );
    expect(state.history).toEqual([
      { at: fixtures.now, games: 1, players: 2 },
      { at: fixtures.now + SampleInterval, games: 0, players: 2 },
    ]);
  });

  it('describes each action of the record', () => {
    expect(describeAction(fixtures.kick)).toEqual({
      tone: 'alert',
      subject: 'Alice',
      verb: 'removed: cheating',
      object: 'local',
    });
    expect(
      describeAction({
        ...fixtures.kick,
        action: 'message',
        target: 'everyone',
        details: { text: 'hi', recipients: 3 },
      }),
    ).toMatchObject({ tone: 'bonus', verb: 'to everyone, 3 received it, by' });
    expect(
      describeAction({
        ...fixtures.kick,
        action: 'ban',
        target: 'address:198.51.100.4',
        details: { reason: '' },
      }),
    ).toMatchObject({ subject: '198.51.100.4', verb: 'banned' });
    expect(
      describeAction({ ...fixtures.kick, action: 'end_game', target: 'game:3', details: { name: 'Duo' } }),
    ).toMatchObject({ subject: 'Duo', verb: 'ended' });
    expect(
      describeAction({
        ...fixtures.kick,
        action: 'unban',
        target: 'address:198.51.100.4',
        details: { reason: 'a mistake' },
      }),
    ).toEqual({ tone: 'bonus', subject: '198.51.100.4', verb: 'ban lifted: a mistake', object: 'local' });
  });
});
