// An administrator signs in to the panel with its account (e2e/master.ts writes it, in [admin] admins), bans
// the account of a player in a game, and sees the player removed; the CSP of the master refuses nothing.

import { spawn } from 'node:child_process';

import { expect, test } from '@playwright/test';

import { AccountPassword, AdminLogin, PlayerName } from './master';

declare global {
  interface Window {
    cspViolations: string[];
  }
}

const origin = (process.env.RTYPE_PANEL_URL ?? '').split('/?')[0] ?? '';

/** A player signed in to its account, in a game of its own, until the test kills it. */
async function playing(): Promise<() => void> {
  const players = spawn(
    process.env.RTYPE_PANEL_PLAYERS ?? '',
    [
      process.env.RTYPE_PANEL_LOBBY_PORT ?? '',
      process.env.RTYPE_PANEL_KEY ?? '',
      'Accounts',
      `${PlayerName}:${AccountPassword}`,
    ],
    { stdio: ['ignore', 'pipe', 'pipe'] },
  );
  let output = '';
  await new Promise<void>((done, fail) => {
    const timer = setTimeout(() => fail(new Error(`The player did not play:\n${output}`)), 20_000);
    const read = (chunk: Buffer) => {
      output += chunk.toString();
      if (/ready \d+/.test(output)) {
        clearTimeout(timer);
        done();
      }
    };
    players.stdout.on('data', read);
    players.stderr.on('data', read);
    players.on('exit', (code) => {
      clearTimeout(timer);
      fail(new Error(`The player exited with ${code}:\n${output}`));
    });
  });
  return () => players.kill();
}

test('an administrator signs in with its account, bans the account of a player, and sees it removed', async ({
  browser,
}) => {
  const stop = await playing();
  const page = await browser.newPage();
  const problems: string[] = [];
  await page.addInitScript(() => {
    window.cspViolations = [];
    document.addEventListener('securitypolicyviolation', (event) => {
      window.cspViolations.push(`${event.violatedDirective} ${event.blockedURI}`);
    });
  });
  page.on('console', (message) => {
    if (message.type() === 'error') problems.push(message.text());
  });
  page.on('pageerror', (error) => problems.push(error.message));

  try {
    // Signed in with its account, not the token
    await page.goto(`${origin}/`);
    const form = page.getByRole('form', { name: 'Sign in' });
    await form.getByRole('textbox', { name: 'Username or email' }).fill(AdminLogin);
    await form.getByLabel('Password').fill(AccountPassword);
    await form.getByRole('button', { name: 'Sign in' }).click();
    await expect(page.getByTestId('connection')).toHaveAttribute('data-state', 'open');
    await expect(page.getByTestId('who')).toContainText('Signed in as Admin');

    // The player, signed in to its account, plays
    await page.getByRole('navigation').getByRole('link', { name: 'Players' }).click();
    const players = page.getByRole('table', { name: 'Players connected' });
    await expect(players).toContainText(PlayerName);

    // Its account banned: it leaves the server at once
    await page.getByRole('navigation').getByRole('link', { name: 'Accounts' }).click();
    const administrator = page.getByRole('row', { name: /Admin/ });
    await expect(administrator).toContainText('Settings');
    await page.getByRole('button', { name: `Ban ${PlayerName}` }).click();
    const dialog = page.getByRole('alertdialog', { name: 'Ban an account' });
    await dialog.getByRole('textbox').fill('end to end');
    await dialog.getByRole('button', { name: 'Ban the account' }).click();
    await expect(page.getByRole('row', { name: new RegExp(PlayerName) })).toContainText('Banned');

    await page.getByRole('navigation').getByRole('link', { name: 'Players' }).click();
    await expect(players).not.toContainText(PlayerName);
    await page.getByRole('navigation').getByRole('link', { name: 'History' }).click();
    const record = page.getByRole('table', { name: 'Record' });
    await expect(record.locator('[data-action="ban_account"]')).toContainText(PlayerName);
    await expect(record.locator('[data-action="ban_account"]')).toContainText('end to end');

    // Signed out, the page to sign in again
    await page.getByTestId('who').getByRole('button', { name: 'Sign out' }).click();
    await expect(page.getByRole('form', { name: 'Sign in' })).toBeVisible();

    expect(await page.evaluate(() => window.cspViolations)).toEqual([]);
    // The refusals of the master before the sign-in and after the sign-out, 401 to the API and to the
    // WebSocket, are the only errors of its console
    const refusal = (problem: string) => problem.includes('401') || problem.includes('/api/events');
    expect(problems.filter((problem) => !refusal(problem))).toEqual([]);
  } finally {
    stop();
    await page.close();
  }
});
