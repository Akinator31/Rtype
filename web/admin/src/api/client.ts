// The HTTP side of the administration API: reading with the session cookie, acting with the CSRF token of
// the session too (docs/protocol/admin-api.md, 2).

import type { Action, Ban, Game, Player, ServerInfo } from './types';

/** A refused request: its status, and the error the master wrote for a person. */
export class ApiError extends Error {
  readonly status: number;

  constructor(status: number, message: string) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }

  /** No session, or one that expired: the panel must be opened again from admin-url.txt. */
  get signedOut(): boolean {
    return this.status === 401;
  }
}

let csrfToken: Promise<string> | null = null;

/** The CSRF token of the session, asked once. */
function csrf(): Promise<string> {
  csrfToken ??= request<{ csrfToken: string }>('GET', '/api/session').then((session) => session.csrfToken);
  // Asked again after a failure, such as a session opened meanwhile
  csrfToken.catch(() => {
    csrfToken = null;
  });
  return csrfToken;
}

/** Forgets the CSRF token, for the tests. */
export function resetSession(): void {
  csrfToken = null;
}

async function request<T>(method: 'GET' | 'POST', path: string, body?: unknown): Promise<T> {
  const headers: Record<string, string> = { Accept: 'application/json' };
  if (method === 'POST') {
    headers['Content-Type'] = 'application/json';
    headers['X-CSRF-Token'] = await csrf();
  }
  let response: Response;
  try {
    response = await fetch(new URL(path, window.location.origin), {
      method,
      headers,
      credentials: 'same-origin',
      body: body === undefined ? undefined : JSON.stringify(body),
    });
  } catch {
    throw new ApiError(0, 'The server cannot be reached');
  }
  const text = await response.text();
  let parsed: unknown = null;
  if (text !== '') {
    try {
      parsed = JSON.parse(text);
    } catch {
      parsed = null;
    }
  }
  if (!response.ok) {
    const message =
      parsed !== null && typeof parsed === 'object' && 'error' in parsed && typeof parsed.error === 'string'
        ? parsed.error
        : `The server answered ${response.status}`;
    throw new ApiError(response.status, message);
  }
  return parsed as T;
}

export const api = {
  server: () => request<ServerInfo>('GET', '/api/server'),
  games: () => request<Game[]>('GET', '/api/games'),
  game: (id: number) => request<Game>('GET', `/api/games/${id}`),
  players: () => request<Player[]>('GET', '/api/players'),
  actions: (options: { limit?: number; before?: number } = {}) => {
    const query = new URLSearchParams();
    if (options.limit !== undefined) query.set('limit', String(options.limit));
    if (options.before !== undefined) query.set('before', String(options.before));
    const suffix = query.size > 0 ? `?${query.toString()}` : '';
    return request<Action[]>('GET', `/api/actions${suffix}`);
  },
  bans: () => request<Ban[]>('GET', '/api/bans'),

  kick: (playerId: number, reason: string) =>
    request<{ playerId: number }>('POST', `/api/players/${playerId}/kick`, { reason }),
  endGame: (gameId: number, reason: string) =>
    request<{ gameId: number }>('POST', `/api/games/${gameId}/end`, { reason }),
  message: (text: string, gameId: number | null) =>
    request<{ recipients: number }>('POST', '/api/messages', { text, gameId }),
  ban: (address: string, duration: number, reason: string) =>
    request<{ ban: Ban; removed: number }>('POST', '/api/bans', { address, duration, reason }),
  unban: (address: string, reason: string) =>
    request<{ address: string }>('POST', '/api/bans/lift', { address, reason }),
};

/** Trades a local token for a session: the master sets its cookie, then sends the browser to the panel. */
export const session = {
  open(token: string): void {
    window.location.assign(`/?token=${encodeURIComponent(token)}`);
  },
};
