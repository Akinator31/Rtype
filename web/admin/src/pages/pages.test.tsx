import { screen, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { HttpResponse, http } from 'msw';
import { describe, expect, it, vi } from 'vitest';

import { session } from '../api/client';
import type { Action, LiveEvent, Player } from '../api/types';
import { setPreferences } from '../preferences';
import * as fixtures from '../test/fixtures';
import { FakeSocket, renderPanel } from '../test/render';
import { sent, server } from '../test/server';

const snapshot: LiveEvent = {
  type: 'snapshot',
  server: fixtures.server,
  games: [fixtures.game],
  players: fixtures.players,
};

const message: Action = {
  ...fixtures.kick,
  id: 20,
  action: 'message',
  target: 'game:3',
  details: { text: 'Last wave, well done', recipients: 2 },
};

describe('the shell', () => {
  it('says when the panel is live, lights the page shown, and says when the connection is lost', async () => {
    const user = userEvent.setup();
    const { socket, store } = renderPanel('/', [snapshot]);
    expect(screen.getByTestId('connection')).toHaveAttribute('data-state', 'open');
    expect(screen.getByRole('link', { name: 'Overview' })).toHaveAttribute('aria-current', 'page');
    expect(screen.getByRole('heading', { name: 'Overview' })).toBeInTheDocument();
    expect(screen.getByTestId('master-state')).toHaveTextContent('Online');

    socket.close();
    expect(await screen.findByRole('alert')).toHaveTextContent('Connection lost');
    expect(screen.getByTestId('connection')).toHaveAttribute('data-state', 'reconnecting');
    expect(screen.getByTestId('master-state')).toHaveTextContent('Offline');
    // The top bar says when it tries again; the overview keeps the last events
    expect(screen.getByText(/^in \d+ s$/)).toBeInTheDocument();
    expect(screen.getByRole('region', { name: 'Last events received' })).toBeInTheDocument();

    const retry = vi.spyOn(store, 'retryNow');
    await user.click(screen.getByRole('button', { name: 'Retry' }));
    expect(retry).toHaveBeenCalledTimes(1);
  });

  it('says an unknown address is not a page, and takes the old address of the record', () => {
    renderPanel('/nowhere', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Not found' })).toBeInTheDocument();
  });

  it('asks for the token of admin-url.txt without a session, and trades it', async () => {
    const user = userEvent.setup();
    const open = vi.spyOn(session, 'open').mockImplementation(() => undefined);
    renderPanel('/games', [], { signedOut: true });
    const form = await screen.findByRole('form', { name: 'Open a session' });
    const field = within(form).getByRole('textbox', { name: 'Address or token' });

    await user.type(field, 'not a token');
    await user.click(within(form).getByRole('button', { name: 'Open the session' }));
    expect(within(form).getByText('Not a token: 64 hexadecimal digits')).toBeInTheDocument();
    expect(open).not.toHaveBeenCalled();

    const token = 'ab'.repeat(32);
    await user.clear(field);
    await user.type(field, `http://127.0.0.1:8080/?token=${token}`);
    await user.click(within(form).getByRole('button', { name: 'Open the session' }));
    expect(open).toHaveBeenCalledWith(token);
    open.mockRestore();
  });

  it('reads the token from admin-url.txt itself', async () => {
    const user = userEvent.setup();
    renderPanel('/', [], { signedOut: true });
    const token = 'cd'.repeat(32);
    const file = new File([`http://127.0.0.1:8080/?token=${token}\n`], 'admin-url.txt', {
      type: 'text/plain',
    });
    await user.upload(await screen.findByTestId('admin-url-file'), file);
    expect(await screen.findByDisplayValue(token)).toBeInTheDocument();
  });

  it('keeps the page under a window once the session expired', async () => {
    let holds = true;
    const { socket, store } = renderPanel('/players', [snapshot], {
      hasSession: () => Promise.resolve(holds),
    });
    // The master restarted: the new WebSocket is refused, the session with it
    holds = false;
    socket.close();
    await waitFor(() =>
      expect(screen.getByTestId('connection')).toHaveAttribute('data-state', 'reconnecting'),
    );
    store.retryNow();
    FakeSocket.last().close();
    const dialog = await screen.findByRole('alertdialog', { name: 'Session expired' });
    expect(within(dialog).getByRole('button', { name: 'Reopen the session' })).toBeInTheDocument();
    // The page stays under it
    expect(screen.getByTestId('player-7')).toBeInTheDocument();
  });
});

describe('the overview', () => {
  it('shows the counts, each opening its page, the games and the events as they come', async () => {
    const { push } = renderPanel('/', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Overview' })).toBeInTheDocument();
    const players = screen.getByRole('link', { name: 'Players connected' });
    expect(players).toHaveTextContent('2 in a game · 1 in the lobby');
    expect(players).toHaveAttribute('href', '/players');
    expect(await screen.findByRole('link', { name: 'Banned addresses' })).toHaveTextContent('1');
    expect(
      within(screen.getByRole('table', { name: 'Games running' })).getByText('Bydo hunters'),
    ).toBeInTheDocument();

    push({ type: 'game.updated', game: { ...fixtures.game, stale: true } });
    expect(screen.getByRole('link', { name: 'Games running' })).toHaveTextContent('1 instance stuck');

    push({ type: 'game.ended', gameId: 3, reason: 'crashed' });
    const feed = screen.getByRole('log', { name: 'Live' });
    expect(within(feed).getAllByText('Bydo hunters')[0]).toBeInTheDocument();
    expect(within(feed).getByText('crashed')).toBeInTheDocument();
  });

  it('lists the latest actions, those of the WebSocket first', async () => {
    const { push } = renderPanel('/', [snapshot]);
    const latest = await screen.findByRole('list', { name: 'Latest actions' });
    await waitFor(() => expect(within(latest).getByText('Alice #7')).toBeInTheDocument());
    push({ type: 'action', action: message });
    expect(within(latest).getAllByRole('listitem')[0]).toHaveTextContent('Last wave, well done');
  });
});

describe('the games', () => {
  it('lists the running games, updated live', () => {
    const { push } = renderPanel('/games', [snapshot]);
    const row = screen.getByTestId('game-3');
    expect(within(row).getByRole('link', { name: 'Bydo hunters' })).toHaveAttribute('href', '/games/3');
    expect(within(row).getByText('2 / 4')).toBeInTheDocument();
    expect(within(row).getByText('Running')).toBeInTheDocument();

    push({ type: 'game.updated', game: { ...fixtures.game, wave: 4, stale: true, statusAge: 4800 } });
    expect(within(screen.getByTestId('game-3')).getByText('04')).toBeInTheDocument();
    expect(within(screen.getByTestId('game-3')).getByText('Stuck')).toBeInTheDocument();
    expect(screen.getByRole('alert')).toHaveTextContent('Bydo hunters #3 sent no status for 4.8 s');

    push({ type: 'game.ended', gameId: 3, reason: 'empty' });
    expect(screen.getByText('No game is running.')).toBeInTheDocument();
  });

  it('filters them by name or id, and by state', async () => {
    const user = userEvent.setup();
    const stuck = { ...fixtures.game, id: 5, name: 'Full house', stale: true };
    renderPanel('/games', [{ ...snapshot, games: [fixtures.game, stuck] } as LiveEvent]);
    expect(screen.getByRole('button', { name: 'Stuck · 1' })).toHaveAttribute('aria-pressed', 'false');

    const search = screen.getByRole('searchbox', { name: 'Search a game' });
    await user.type(search, 'bydo');
    expect(screen.getByTestId('game-3')).toBeInTheDocument();
    expect(screen.queryByTestId('game-5')).not.toBeInTheDocument();
    await user.clear(search);
    await user.type(search, '#5');
    expect(screen.queryByTestId('game-3')).not.toBeInTheDocument();
    await user.clear(search);

    await user.click(screen.getByRole('button', { name: 'Stuck · 1' }));
    expect(screen.queryByTestId('game-3')).not.toBeInTheDocument();
    expect(screen.getByTestId('game-5')).toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'Running · 1' }));
    expect(screen.getByTestId('game-3')).toBeInTheDocument();
    expect(screen.queryByTestId('game-5')).not.toBeInTheDocument();

    await user.type(search, 'nothing');
    expect(screen.getByText('No game matches the search.')).toBeInTheDocument();
  });

  it('sends the host to the messages of a game', async () => {
    const user = userEvent.setup();
    const { router } = renderPanel('/games', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Message Bydo hunters' }));
    expect(router.state.location.pathname).toBe('/messages');
    expect(router.state.location.search).toBe('?to=3');
  });
});

describe('a game', () => {
  it('shows its pilots, their address, score, state and latency', () => {
    renderPanel('/games/3', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Bydo hunters · #3' })).toBeInTheDocument();
    const alice = screen.getByTestId('pilot-7');
    expect(within(alice).getByText('203.0.113.9')).toBeInTheDocument();
    expect(within(alice).getByText('0001500')).toBeInTheDocument();
    expect(within(alice).getByText('Alive')).toBeInTheDocument();
    expect(within(alice).getByText('35 ms')).toBeInTheDocument();
    // Bob's client is not connected yet: no status for him
    const bob = screen.getByTestId('pilot-9');
    expect(within(bob).getByText('not connected')).toBeInTheDocument();
    expect(within(bob).getByText('Joining')).toBeInTheDocument();
    expect(within(bob).getByText('Not measured')).toBeInTheDocument();
  });

  it('is ended after its confirmation, with a reason', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/games/3', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'End the game' }));
    const dialog = await screen.findByRole('alertdialog', { name: 'End a game' });
    await user.type(within(dialog).getByRole('textbox', { name: 'Reason' }), 'maintenance');
    await user.click(within(dialog).getByRole('button', { name: 'End the game' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/games/3/end',
      csrf: fixtures.csrfToken,
      body: { reason: 'maintenance' },
    });
    expect(await screen.findByText('Bydo hunters #3 · maintenance')).toBeInTheDocument();

    push({ type: 'game.ended', gameId: 3, reason: 'ended_by_admin' });
    expect(await screen.findByText(/This game is not running/)).toBeInTheDocument();
  });

  it('is ended at once when the host chose not to confirm', async () => {
    const user = userEvent.setup();
    setPreferences({ confirmActions: false });
    renderPanel('/games/3', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'End the game' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument();
  });

  it('sends nothing when the dialog is cancelled', async () => {
    const user = userEvent.setup();
    renderPanel('/games/3', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'End the game' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.click(within(dialog).getByRole('button', { name: 'Cancel' }));
    await waitFor(() => expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument());
    expect(sent).toEqual([]);
  });

  it('keeps the dialog open with the error of the master', async () => {
    const user = userEvent.setup();
    server.use(
      http.post('*/api/games/:id/end', () => HttpResponse.json({ error: 'no such game' }, { status: 404 })),
    );
    renderPanel('/games/3', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'End the game' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.click(within(dialog).getByRole('button', { name: 'End the game' }));
    expect(await within(dialog).findByText('no such game')).toBeInTheDocument();
    expect(screen.getByRole('alertdialog')).toBeInTheDocument();
  });

  it('messages its players, and shows its own events', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/games/3', [snapshot]);
    const card = screen.getByRole('form', { name: 'Message to the game' });
    const send = within(card).getByRole('button', { name: 'Send to 2 players' });
    expect(send).toBeDisabled();
    await user.type(within(card).getByRole('textbox'), 'Last wave');
    await user.click(send);
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]?.body).toEqual({ text: 'Last wave', gameId: 3 });
    expect(within(card).getByRole('textbox')).toHaveValue('');

    push({ type: 'game.updated', game: { ...fixtures.game, wave: 3 } });
    push({ type: 'player.joined', player: { ...(fixtures.players[1] as Player), id: 30 } });
    const events = screen.getByRole('log', { name: 'Events of the game' });
    expect(events).toHaveTextContent('wave 3 reached');
    expect(events).not.toHaveTextContent('joined the lobby');
  });

  it('says when it is not running', () => {
    renderPanel('/games/42', [snapshot]);
    expect(screen.getByText(/This game is not running/)).toBeInTheDocument();
  });
});

describe('the players', () => {
  it('lists the players of the lobby and their game, and filters them', async () => {
    const user = userEvent.setup();
    renderPanel('/players', [snapshot]);
    const carol = screen.getByTestId('player-8');
    expect(within(carol).getByText('198.51.100.4')).toBeInTheDocument();
    expect(within(carol).getByText('Lobby')).toBeInTheDocument();
    expect(
      within(screen.getByTestId('player-7')).getByRole('link', { name: 'Bydo hunters #3' }),
    ).toBeInTheDocument();

    await user.click(screen.getByRole('button', { name: 'Lobby · 1' }));
    expect(screen.queryByTestId('player-7')).not.toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'All · 3' }));
    await user.type(screen.getByRole('searchbox'), '203.0.113');
    expect(screen.queryByTestId('player-8')).not.toBeInTheDocument();
    expect(screen.getByTestId('player-9')).toBeInTheDocument();
  });

  it('shows the card of the player chosen, and who shares its address', async () => {
    const user = userEvent.setup();
    const twin = { ...(fixtures.players[1] as Player), id: 11, name: 'Dana' };
    renderPanel('/players', [{ ...snapshot, players: [...fixtures.players, twin] } as LiveEvent]);
    expect(screen.queryByTestId('pilot-card')).not.toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'Carol' }));
    const card = screen.getByTestId('pilot-card');
    expect(within(card).getByText('Carol')).toBeInTheDocument();
    expect(within(card).getByRole('note')).toHaveTextContent('DANA #11 plays from the same address.');
    // Its line closes it again
    await user.click(screen.getByRole('button', { name: 'Carol' }));
    expect(screen.queryByTestId('pilot-card')).not.toBeInTheDocument();
  });

  it('removes a player after its confirmation', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Remove Alice' }));
    const dialog = await screen.findByRole('alertdialog', { name: 'Remove a player' });
    await user.type(within(dialog).getByRole('textbox'), 'cheating');
    await user.click(within(dialog).getByRole('button', { name: 'Remove' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/players/7/kick',
      csrf: fixtures.csrfToken,
      body: { reason: 'cheating' },
    });
    expect(await screen.findByText('Alice #7 · cheating')).toBeInTheDocument();

    push({ type: 'player.left', playerId: 7 });
    expect(screen.queryByTestId('player-7')).not.toBeInTheDocument();
  });

  it('bans the address of a player, for the time chosen, and says who it removes', async () => {
    const user = userEvent.setup();
    renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Ban 198.51.100.4' }));
    const dialog = await screen.findByRole('alertdialog', { name: 'Ban an address' });
    expect(within(dialog).getByRole('textbox', { name: 'IP address' })).toHaveValue('198.51.100.4');
    expect(within(dialog).getByTestId('removed-pilots')).toHaveTextContent('1 pilot will be removed');
    await user.click(within(dialog).getByRole('button', { name: '24 h' }));
    await user.click(within(dialog).getByRole('button', { name: 'Ban 1 d' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/bans',
      csrf: fixtures.csrfToken,
      body: { address: '198.51.100.4', duration: 86400, reason: '' },
    });
    expect(await screen.findByText('198.51.100.4 · 1 d · 1 player removed')).toBeInTheDocument();
  });

  it('takes any duration of a ban, within its bounds, and refuses a reason the server would refuse', async () => {
    const user = userEvent.setup();
    renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Ban 198.51.100.4' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.click(within(dialog).getByRole('button', { name: 'Other' }));
    const seconds = within(dialog).getByRole('textbox', { name: 'Duration in seconds' });
    await user.clear(seconds);
    await user.type(seconds, '900000');
    expect(within(dialog).getByText('Between 1 s and 604 800 s (7 days)')).toBeInTheDocument();
    await user.clear(seconds);
    await user.type(seconds, '90');
    expect(within(dialog).getByRole('button', { name: 'Ban 90 s' })).toBeEnabled();

    await user.click(within(dialog).getByRole('textbox', { name: 'Reason' }));
    await user.paste('é'.repeat(101));
    expect(within(dialog).getByRole('button', { name: 'Ban 90 s' })).toBeDisabled();
  });
});

describe('the bans', () => {
  it('lists the addresses banned, and lifts a ban after its confirmation', async () => {
    const user = userEvent.setup();
    let bans = [{ ...fixtures.ban, expiresAt: Date.now() + 3_600_000 }];
    server.use(http.get('*/api/bans', () => HttpResponse.json(bans)));
    const { push } = renderPanel('/bans', [snapshot]);
    const row = await screen.findByTestId('ban-198.51.100.4');
    expect(within(row).getByText('flood')).toBeInTheDocument();
    expect(within(row).getByRole('meter')).toBeInTheDocument();

    await user.click(within(row).getByRole('button', { name: 'Lift 198.51.100.4' }));
    const dialog = await screen.findByRole('alertdialog', { name: 'Lift a ban' });
    await user.type(within(dialog).getByRole('textbox'), 'a mistake');
    await user.click(within(dialog).getByRole('button', { name: 'Lift the ban' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/bans/lift',
      csrf: fixtures.csrfToken,
      body: { address: '198.51.100.4', reason: 'a mistake' },
    });

    // The action recorded comes back on the WebSocket: the bans are asked again
    bans = [];
    const lifted: Action = {
      ...fixtures.kick,
      id: 13,
      at: Date.now(),
      action: 'unban',
      target: 'address:198.51.100.4',
      details: { reason: 'a mistake' },
    };
    server.use(http.get('*/api/actions', () => HttpResponse.json([lifted])));
    push({ type: 'action', action: lifted });
    expect(await screen.findByText('No address is banned.')).toBeInTheDocument();
    expect(
      within(screen.getByRole('table', { name: 'Lifted or expired lately' })).getByText('a mistake'),
    ).toBeInTheDocument();
  });

  it('bans an address from its form', async () => {
    const user = userEvent.setup();
    renderPanel('/bans', [snapshot]);
    const form = screen.getByRole('form', { name: 'New ban' });
    const address = within(form).getByRole('textbox', { name: 'IP address' });
    await user.type(address, '198.51.100.4:4242');
    expect(within(form).getByText('An address without a port, IPv4 or IPv6')).toBeInTheDocument();
    expect(within(form).getByRole('button', { name: 'Ban' })).toBeDisabled();
    await user.clear(address);
    await user.type(address, '198.51.100.4');
    expect(within(form).getByTestId('removed-pilots')).toHaveTextContent('Carol #8');
    await user.click(within(form).getByRole('button', { name: '10 min' }));
    await user.click(within(form).getByRole('button', { name: 'Ban' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]?.body).toEqual({ address: '198.51.100.4', duration: 600, reason: '' });
    await waitFor(() => expect(address).toHaveValue(''));
  });
});

describe('the messages', () => {
  it('sends a message to the game chosen, from a template', async () => {
    const user = userEvent.setup();
    renderPanel('/messages?to=3', [snapshot]);
    const form = screen.getByRole('form', { name: 'New message' });
    expect(within(form).getByRole('button', { name: 'Bydo hunters #3 · 2' })).toHaveAttribute(
      'aria-pressed',
      'true',
    );
    await user.click(within(form).getByRole('button', { name: 'Restart in 5 minutes' }));
    expect(within(form).getByRole('img', { name: 'Preview in game' })).toHaveTextContent(
      'Restart in 5 minutes',
    );
    await user.click(within(form).getByRole('button', { name: 'Send to 2 players' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]?.body).toEqual({ text: 'Restart in 5 minutes', gameId: 3 });
    expect(await screen.findByText('2 recipients · Bydo hunters #3')).toBeInTheDocument();
  });

  it('sends to everyone, and keeps a text as a template', async () => {
    const user = userEvent.setup();
    renderPanel('/messages', [snapshot]);
    const form = screen.getByRole('form', { name: 'New message' });
    await user.type(within(form).getByRole('textbox'), 'Tournament at 21:00');
    await user.click(within(form).getByRole('button', { name: '+ Save' }));
    expect(within(form).getByRole('button', { name: 'Tournament at 21:00' })).toBeInTheDocument();
    await user.click(within(form).getByRole('button', { name: 'Send to 3 players' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]?.body).toEqual({ text: 'Tournament at 21:00', gameId: null });
  });

  it('lists the messages sent, from the record', async () => {
    server.use(http.get('*/api/actions', () => HttpResponse.json([message, fixtures.kick])));
    renderPanel('/messages', [snapshot]);
    const table = await screen.findByRole('table', { name: 'Messages sent' });
    expect(within(table).getByText('Bydo hunters #3')).toBeInTheDocument();
    expect(within(table).getByText('Last wave, well done')).toBeInTheDocument();
    expect(within(table).queryByText('Alice')).not.toBeInTheDocument();
  });
});

describe('the history', () => {
  it('shows the record, the actions of the WebSocket first, filtered by kind', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/history', [snapshot]);
    const record = await screen.findByRole('table', { name: 'Record' });
    await waitFor(() => expect(within(record).getByText('Alice #7')).toBeInTheDocument());
    expect(within(record).getByText('Alice · 203.0.113.9 · game #3 · cheating')).toBeInTheDocument();

    push({ type: 'action', action: message });
    const rows = within(screen.getByRole('table', { name: 'Record' })).getAllByRole('row');
    expect(rows[1]).toHaveAttribute('data-action', 'message');

    await user.click(screen.getByRole('button', { name: 'Kick' }));
    expect(
      within(screen.getByRole('table', { name: 'Record' })).queryByText('game:3'),
    ).not.toBeInTheDocument();
  });

  it('unfolds an entry into its details', async () => {
    const user = userEvent.setup();
    renderPanel('/history', [snapshot]);
    await user.click(await screen.findByRole('button', { name: 'Details 12' }));
    const details = screen.getByTestId('entry-details');
    expect(within(details).getByText('reason')).toBeInTheDocument();
    expect(within(details).getByText('cheating')).toBeInTheDocument();
  });

  it('pages the older actions, then the newer again', async () => {
    const user = userEvent.setup();
    const page = Array.from({ length: 100 }, (_, index) => ({ ...fixtures.kick, id: 200 - index }));
    const asked: (string | null)[] = [];
    server.use(
      http.get('*/api/actions', ({ request }) => {
        const before = new URL(request.url).searchParams.get('before');
        asked.push(before);
        return HttpResponse.json(before === null ? page : [{ ...fixtures.kick, id: 3 }]);
      }),
    );
    renderPanel('/history', [snapshot]);
    await user.click(await screen.findByRole('button', { name: 'Older' }));
    await waitFor(() => expect(screen.getAllByRole('row')).toHaveLength(2));
    expect(asked).toContain('101');
    // The last page was short: nothing older
    expect(screen.queryByRole('button', { name: 'Older' })).not.toBeInTheDocument();
    await user.click(screen.getByRole('button', { name: 'Newer' }));
    await waitFor(() => expect(screen.getAllByRole('row')).toHaveLength(101));
  });

  it('is where the record used to be', () => {
    const { router } = renderPanel('/actions', [snapshot]);
    expect(router.state.location.pathname).toBe('/history');
  });
});

describe('the settings', () => {
  it('keeps the preferences of the panel', async () => {
    const user = userEvent.setup();
    renderPanel('/settings', [snapshot]);
    const confirm = screen.getByRole('switch', { name: 'Confirm the actions' });
    expect(confirm).toHaveAttribute('aria-checked', 'true');
    await user.click(confirm);
    expect(confirm).toHaveAttribute('aria-checked', 'false');
    expect(JSON.parse(window.localStorage.getItem('rtype-panel-preferences') ?? '{}')).toMatchObject({
      confirmActions: false,
    });
  });
});
