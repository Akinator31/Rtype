import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import * as fixtures from '../test/fixtures';
import { FakeSocket, fakeStore } from '../test/render';
import { ReconnectDelays } from './store';

describe('the store of the WebSocket', () => {
  beforeEach(() => {
    vi.useFakeTimers();
  });
  afterEach(() => {
    vi.useRealTimers();
  });

  it('opens /api/events on the address of the page, then applies its frames', () => {
    const store = fakeStore();
    store.start();
    const socket = FakeSocket.last();
    expect(socket.url).toBe(`ws://${window.location.host}/api/events`);
    expect(store.getSnapshot().connection).toBe('connecting');

    socket.open();
    socket.send({
      type: 'snapshot',
      server: fixtures.server,
      games: [fixtures.game],
      players: fixtures.players,
    });
    expect(store.getSnapshot().connection).toBe('open');
    expect(store.getSnapshot().players).toHaveLength(3);
    store.stop();
  });

  it('ignores a frame that is not JSON', () => {
    const store = fakeStore();
    store.start();
    const socket = FakeSocket.last();
    socket.open();
    const before = store.getSnapshot();
    socket.onmessage?.({ data: 'not json' });
    expect(store.getSnapshot()).toBe(before);
    store.stop();
  });

  it('reconnects after a delay that grows, and gets a new snapshot', async () => {
    const store = fakeStore();
    store.start();
    FakeSocket.last().open();
    for (const [attempt, delay] of ReconnectDelays.entries()) {
      FakeSocket.last().close();
      expect(store.getSnapshot().connection).toBe('reconnecting');
      // A close before it opened asks whether the session holds: it does
      await vi.advanceTimersByTimeAsync(delay - 1);
      expect(FakeSocket.opened).toHaveLength(attempt + 1);
      await vi.advanceTimersByTimeAsync(1);
      expect(FakeSocket.opened).toHaveLength(attempt + 2);
    }
    // Once open again, the next loss waits the first delay
    FakeSocket.last().open();
    expect(store.getSnapshot().connection).toBe('open');
    FakeSocket.last().close();
    await vi.advanceTimersByTimeAsync(ReconnectDelays[0] ?? 0);
    expect(FakeSocket.opened).toHaveLength(ReconnectDelays.length + 2);
    store.stop();
  });

  it('stops trying once the session is gone', async () => {
    const store = fakeStore({ hasSession: () => Promise.resolve(false) });
    store.start();
    // Refused before it opened: the master answers the upgrade 401
    FakeSocket.last().close();
    await vi.advanceTimersByTimeAsync(60_000);
    expect(store.getSnapshot().connection).toBe('signed-out');
    expect(FakeSocket.opened).toHaveLength(1);
  });

  it('closes its WebSocket when stopped, and opens no other', async () => {
    const store = fakeStore();
    store.start();
    const socket = FakeSocket.last();
    socket.open();
    store.stop();
    expect(socket.closed).toBe(true);
    await vi.advanceTimersByTimeAsync(60_000);
    expect(FakeSocket.opened).toHaveLength(1);
  });
});
