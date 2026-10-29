import { HttpResponse, http } from 'msw';
import { describe, expect, it } from 'vitest';

import * as fixtures from '../test/fixtures';
import { sent, server } from '../test/server';
import { ApiError, api } from './client';

describe('the client of the API', () => {
  it('reads with the session alone', async () => {
    expect(await api.server()).toEqual(fixtures.server);
    expect(await api.games()).toEqual([fixtures.game]);
  });

  it('asks the CSRF token of the session once, and sends it with every action', async () => {
    let asked = 0;
    server.use(
      http.get('*/api/session', () => {
        asked += 1;
        return HttpResponse.json({ csrfToken: fixtures.csrfToken });
      }),
    );
    await api.kick(7, 'cheating');
    await api.message('Hello', null);
    expect(await api.unban('198.51.100.4', '')).toEqual({ address: '198.51.100.4' });
    expect(asked).toBe(1);
    expect(sent).toEqual([
      { path: '/api/players/7/kick', csrf: fixtures.csrfToken, body: { reason: 'cheating' } },
      { path: '/api/messages', csrf: fixtures.csrfToken, body: { text: 'Hello', gameId: null } },
      { path: '/api/bans/lift', csrf: fixtures.csrfToken, body: { address: '198.51.100.4', reason: '' } },
    ]);
  });

  it('pages the record of the actions', async () => {
    let query = '';
    server.use(
      http.get('*/api/actions', ({ request }) => {
        query = new URL(request.url).search;
        return HttpResponse.json([]);
      }),
    );
    await api.actions({ limit: 50, before: 12 });
    expect(query).toBe('?limit=50&before=12');
  });

  it('gives the error of the master, and says when the session is gone', async () => {
    server.use(
      http.get('*/api/games/:id', () => HttpResponse.json({ error: 'no such game' }, { status: 404 })),
    );
    await expect(api.game(99)).rejects.toMatchObject({
      status: 404,
      message: 'no such game',
      signedOut: false,
    });

    server.use(
      http.get('*/api/server', () => HttpResponse.json({ error: 'no session' }, { status: 401 })),
      http.get('*/api/session', () => HttpResponse.json({ error: 'no session' }, { status: 401 })),
    );
    const refused = await api.server().catch((error: unknown) => error);
    expect(refused).toBeInstanceOf(ApiError);
    expect((refused as ApiError).signedOut).toBe(true);
    // No action leaves without its token
    await expect(api.endGame(3, '')).rejects.toMatchObject({ status: 401 });
    expect(sent).toEqual([]);
  });

  it('says when the server cannot be reached', async () => {
    server.use(http.get('*/api/server', () => HttpResponse.error()));
    await expect(api.server()).rejects.toMatchObject({ status: 0 });
  });
});
