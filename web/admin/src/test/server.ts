// The administration API mocked with MSW: each route answers the fixtures, and every POST is kept, with its
// headers, for the tests to check what the panel sent.

import { HttpResponse, http } from 'msw';
import { setupServer } from 'msw/node';

import * as fixtures from './fixtures';

export interface Sent {
  path: string;
  csrf: string | null;
  body: unknown;
}

export const sent: Sent[] = [];

async function keep(request: Request): Promise<void> {
  const text = await request.text();
  sent.push({
    path: new URL(request.url).pathname,
    csrf: request.headers.get('X-CSRF-Token'),
    body: text === '' ? null : JSON.parse(text),
  });
}

/** A POST without the token of the session is refused, as the master does. */
function checked(answer: (request: Request) => Response | Promise<Response>) {
  return async ({ request }: { request: Request }) => {
    if (request.headers.get('X-CSRF-Token') !== fixtures.csrfToken) {
      return HttpResponse.json({ error: 'no CSRF token' }, { status: 403 });
    }
    await keep(request.clone());
    return answer(request);
  };
}

/** The id of an account, in its path. */
const accountId = (request: Request) => Number(new URL(request.url).pathname.split('/')[3]);

export const handlers = [
  http.get('*/api/session', () => HttpResponse.json({ csrfToken: fixtures.csrfToken, account: null })),
  http.get('*/api/sign-in', () => HttpResponse.json({ password: false, discord: false })),
  http.post('*/api/sign-in', async ({ request }) => {
    await keep(request.clone());
    return HttpResponse.json({ error: 'wrong login or password' }, { status: 401 });
  }),
  http.post(
    '*/api/sign-out',
    checked(() => HttpResponse.json({})),
  ),
  http.get('*/api/accounts', () => HttpResponse.json(fixtures.accounts)),
  http.post(
    '*/api/accounts/:id/ban',
    checked((request) =>
      HttpResponse.json({
        account: {
          ...fixtures.accounts.find((account) => account.id === accountId(request)),
          bannedAt: fixtures.now,
        },
        removed: 1,
      }),
    ),
  ),
  http.post(
    '*/api/accounts/:id/unban',
    checked(() => HttpResponse.json({ account: fixtures.accounts[0] })),
  ),
  http.post(
    '*/api/accounts/:id/role',
    checked(() => HttpResponse.json({ account: fixtures.accounts[0] })),
  ),
  http.post(
    '*/api/accounts/:id/mute-voice',
    checked(async (request) => {
      const body = (await request.json()) as { duration: number; reason: string };
      return HttpResponse.json({
        account: {
          ...fixtures.accounts.find((account) => account.id === accountId(request)),
          voiceMute: {
            accountId: accountId(request),
            createdAt: fixtures.now,
            expiresAt: fixtures.now + body.duration * 1000,
            reason: body.reason,
            actor: 'local',
          },
        },
        removed: 1,
      });
    }),
  ),
  http.post(
    '*/api/accounts/:id/unmute-voice',
    checked(() => HttpResponse.json({ account: fixtures.accounts[0], removed: 1 })),
  ),
  http.post(
    '*/api/accounts/:id/delete',
    checked(() => HttpResponse.json({ account: null, removed: 0 })),
  ),
  http.get('*/api/server', () => HttpResponse.json(fixtures.server)),
  http.get('*/api/games', () => HttpResponse.json([fixtures.game])),
  http.get('*/api/players', () => HttpResponse.json(fixtures.players)),
  http.get('*/api/actions', () => HttpResponse.json([fixtures.kick])),
  http.get('*/api/bans', () => HttpResponse.json([fixtures.ban])),
  http.post(
    '*/api/players/:id/kick',
    checked((request) =>
      HttpResponse.json({ playerId: Number(new URL(request.url).pathname.split('/')[3]) }),
    ),
  ),
  http.post(
    '*/api/players/:id/mute-voice',
    checked((request) =>
      HttpResponse.json({ playerId: Number(new URL(request.url).pathname.split('/')[3]), voiceMuted: true }),
    ),
  ),
  http.post(
    '*/api/players/:id/unmute-voice',
    checked((request) =>
      HttpResponse.json({ playerId: Number(new URL(request.url).pathname.split('/')[3]), voiceMuted: false }),
    ),
  ),
  http.post(
    '*/api/games/:id/end',
    checked((request) => HttpResponse.json({ gameId: Number(new URL(request.url).pathname.split('/')[3]) })),
  ),
  http.post(
    '*/api/messages',
    checked(() => HttpResponse.json({ recipients: 2 })),
  ),
  http.post(
    '*/api/bans',
    checked(() => HttpResponse.json({ ban: fixtures.ban, removed: 1 })),
  ),
  http.post(
    '*/api/bans/lift',
    checked(async (request) =>
      HttpResponse.json({ address: ((await request.json()) as { address: string }).address }),
    ),
  ),
];

export const server = setupServer(...handlers);
