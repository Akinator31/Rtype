import { screen, waitFor, within } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { HttpResponse, http } from 'msw';
import { describe, expect, it } from 'vitest';

import type { LiveEvent } from '../api/types';
import * as fixtures from '../test/fixtures';
import { renderPanel } from '../test/render';
import { sent, server } from '../test/server';

const snapshot: LiveEvent = {
  type: 'snapshot',
  server: fixtures.server,
  games: [fixtures.game],
  players: fixtures.players,
};

describe('the shell', () => {
  it('says when the panel is live, and when the connection is lost', async () => {
    const { socket } = renderPanel('/', [snapshot]);
    expect(screen.getByTestId('connection')).toHaveAttribute('data-state', 'open');
    expect(screen.getByRole('link', { name: 'Overview' })).toHaveAttribute('aria-current', 'page');

    socket.close();
    expect(await screen.findByRole('alert')).toHaveTextContent('Connection lost');
    expect(screen.getByTestId('connection')).toHaveAttribute('data-state', 'reconnecting');
  });

  it('says an unknown address is not a page', () => {
    renderPanel('/nowhere', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Not found' })).toBeInTheDocument();
  });
});

describe('the overview', () => {
  it('shows the counts, the server, and the events as they come', async () => {
    const { push } = renderPanel('/', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Overview' })).toBeInTheDocument();
    // The bans come from /api/server
    expect(await screen.findByText('0.4.0')).toBeInTheDocument();
    expect(screen.getByText('4243–4299')).toBeInTheDocument();
    expect(screen.getByRole('meter', { name: 'In a game' })).toHaveAttribute('aria-valuetext', '2 of 3');

    push({ type: 'game.ended', gameId: 3, reason: 'crashed' });
    const feed = screen.getByRole('log');
    expect(within(feed).getByText('Bydo hunters')).toBeInTheDocument();
    expect(within(feed).getByText('crashed')).toBeInTheDocument();
  });

  it('sends a message to every player', async () => {
    const user = userEvent.setup();
    renderPanel('/', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Message everyone' }));
    const dialog = await screen.findByRole('alertdialog');
    const confirm = within(dialog).getByRole('button', { name: 'Confirm' });
    // Nothing to send yet
    expect(confirm).toBeDisabled();
    await user.type(within(dialog).getByRole('textbox'), 'Restart in 5 minutes');
    await user.click(confirm);
    await waitFor(() => expect(screen.queryByRole('alertdialog')).not.toBeInTheDocument());
    expect(sent).toEqual([
      {
        path: '/api/messages',
        csrf: fixtures.csrfToken,
        body: { text: 'Restart in 5 minutes', gameId: null },
      },
    ]);
    expect(await screen.findByText('2 players received it')).toBeInTheDocument();
  });
});

describe('the games', () => {
  it('lists the running games, updated live', () => {
    const { push } = renderPanel('/games', [snapshot]);
    const row = screen.getByTestId('game-3');
    expect(within(row).getByRole('link', { name: 'Bydo hunters' })).toHaveAttribute('href', '/games/3');
    expect(within(row).getByText('2/4')).toBeInTheDocument();
    expect(within(row).getByText('Live')).toBeInTheDocument();

    push({ type: 'game.updated', game: { ...fixtures.game, wave: 4, stale: true } });
    expect(within(screen.getByTestId('game-3')).getByText('4')).toBeInTheDocument();
    expect(within(screen.getByTestId('game-3')).getByText('Stale')).toBeInTheDocument();

    push({ type: 'game.ended', gameId: 3, reason: 'empty' });
    expect(screen.getByText('No game is running.')).toBeInTheDocument();
  });

  it('filters them by name, and by state', async () => {
    const user = userEvent.setup();
    const full = {
      ...fixtures.game,
      id: 5,
      name: 'Full house',
      maxPlayers: 2,
      stale: true,
      players: fixtures.game.players,
    };
    renderPanel('/games', [{ ...snapshot, games: [fixtures.game, full] } as LiveEvent]);
    expect(screen.getAllByRole('row')).toHaveLength(3);

    await user.type(screen.getByRole('textbox', { name: 'Filter by name' }), 'bydo');
    expect(screen.getByTestId('game-3')).toBeInTheDocument();
    expect(screen.queryByTestId('game-5')).not.toBeInTheDocument();
    await user.clear(screen.getByRole('textbox', { name: 'Filter by name' }));

    await user.selectOptions(screen.getByRole('combobox', { name: 'Status' }), 'stale');
    expect(screen.queryByTestId('game-3')).not.toBeInTheDocument();
    expect(screen.getByTestId('game-5')).toBeInTheDocument();

    await user.selectOptions(screen.getByRole('combobox', { name: 'Status' }), 'room');
    expect(screen.getByTestId('game-3')).toBeInTheDocument();
    expect(screen.queryByTestId('game-5')).not.toBeInTheDocument();

    await user.type(screen.getByRole('textbox', { name: 'Filter by name' }), 'nothing');
    expect(screen.getByText('No game matches the filter.')).toBeInTheDocument();
  });
});

describe('a game', () => {
  it('shows its players, their scores and latency', () => {
    renderPanel('/games/3', [snapshot]);
    expect(screen.getByRole('heading', { name: 'Bydo hunters' })).toBeInTheDocument();
    const table = screen.getByRole('table', { name: 'Players' });
    const alice = within(table).getByText('Alice').closest('tr') as HTMLElement;
    expect(within(alice).getByText('0001500')).toBeInTheDocument();
    expect(within(alice).getByText('35 ms')).toBeInTheDocument();
    // Bob's client is not connected yet: no status for him
    const bob = within(table).getByText('Bob').closest('tr') as HTMLElement;
    expect(within(bob).getAllByText('—')).toHaveLength(3);
  });

  it('is ended after its confirmation, with a reason', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/games/3', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'End the game' }));
    const dialog = await screen.findByRole('alertdialog');
    expect(within(dialog).getByText('End Bydo hunters?')).toBeInTheDocument();
    await user.type(within(dialog).getByRole('textbox'), 'maintenance');
    await user.click(within(dialog).getByRole('button', { name: 'End the game' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/games/3/end',
      csrf: fixtures.csrfToken,
      body: { reason: 'maintenance' },
    });

    push({ type: 'game.ended', gameId: 3, reason: 'ended_by_admin' });
    expect(await screen.findByText(/This game is not running/)).toBeInTheDocument();
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

  it('says when it is not running', () => {
    renderPanel('/games/42', [snapshot]);
    expect(screen.getByText(/This game is not running/)).toBeInTheDocument();
  });
});

describe('the players', () => {
  it('lists the players of the lobby and their game', () => {
    renderPanel('/players', [snapshot]);
    const carol = screen.getByTestId('player-8');
    expect(within(carol).getByText('198.51.100.4')).toBeInTheDocument();
    expect(within(carol).getByText('In the lobby')).toBeInTheDocument();
    expect(
      within(screen.getByTestId('player-7')).getByRole('link', { name: 'Bydo hunters' }),
    ).toBeInTheDocument();
  });

  it('removes a player after its confirmation', async () => {
    const user = userEvent.setup();
    const { push } = renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Remove Alice' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.type(within(dialog).getByRole('textbox'), 'cheating');
    await user.click(within(dialog).getByRole('button', { name: 'Remove' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/players/7/kick',
      csrf: fixtures.csrfToken,
      body: { reason: 'cheating' },
    });
    expect(await screen.findByText('Alice removed')).toBeInTheDocument();

    push({ type: 'player.left', playerId: 7 });
    expect(screen.queryByTestId('player-7')).not.toBeInTheDocument();
  });

  it('bans the address of a player, for the time chosen', async () => {
    const user = userEvent.setup();
    renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Ban 198.51.100.4' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.selectOptions(within(dialog).getByRole('combobox'), '86400');
    await user.click(within(dialog).getByRole('button', { name: 'Ban' }));
    await waitFor(() => expect(sent).toHaveLength(1));
    expect(sent[0]).toEqual({
      path: '/api/bans',
      csrf: fixtures.csrfToken,
      body: { address: '198.51.100.4', duration: 86400, reason: '' },
    });
    expect(await screen.findByText('1 player removed')).toBeInTheDocument();
  });

  it('refuses a reason the server would refuse', async () => {
    const user = userEvent.setup();
    renderPanel('/players', [snapshot]);
    await user.click(screen.getByRole('button', { name: 'Remove Alice' }));
    const dialog = await screen.findByRole('alertdialog');
    await user.click(within(dialog).getByRole('textbox'));
    await user.paste('é'.repeat(101));
    expect(within(dialog).getByRole('button', { name: 'Remove' })).toBeDisabled();
  });
});

describe('the actions', () => {
  it('shows the record, the actions of the WebSocket first, and the bans', async () => {
    const { push } = renderPanel('/actions', [snapshot]);
    const record = await screen.findByRole('list', { name: 'Actions' });
    expect(within(record).getByText('Alice')).toBeInTheDocument();
    expect(within(record).getByText('removed: cheating')).toBeInTheDocument();
    expect(await screen.findByText('flood')).toBeInTheDocument();

    push({
      type: 'action',
      action: { ...fixtures.kick, id: 13, target: 'player:9', details: { name: 'Bob', reason: '' } },
    });
    const items = within(screen.getByRole('list', { name: 'Actions' })).getAllByRole('listitem');
    expect(items).toHaveLength(2);
    expect(items[0]).toHaveTextContent('Bob');
  });

  it('pages the older actions', async () => {
    const user = userEvent.setup();
    const page = Array.from({ length: 50 }, (_, index) => ({ ...fixtures.kick, id: 100 - index }));
    let before: string | null = null;
    server.use(
      http.get('*/api/actions', ({ request }) => {
        before = new URL(request.url).searchParams.get('before');
        return HttpResponse.json(before === null ? page : [{ ...fixtures.kick, id: 3 }]);
      }),
    );
    renderPanel('/actions', [snapshot]);
    await user.click(await screen.findByRole('button', { name: 'Older actions' }));
    await waitFor(() => expect(screen.getAllByRole('listitem')).toHaveLength(51));
    expect(before).toBe('51');
    // The last page was short: nothing older
    expect(screen.queryByRole('button', { name: 'Older actions' })).not.toBeInTheDocument();
  });
});
