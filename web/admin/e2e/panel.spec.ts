// The panel against a real master and a game of real players (e2e/master.ts): opened with the address of
// admin-url.txt, it shows the game live and ends it; and the CSP of the master refuses nothing it does.

import { expect, test, type Page } from '@playwright/test';

declare global {
  interface Window {
    cspViolations: string[];
  }
}

const url = process.env.RTYPE_PANEL_URL ?? '';
const gameId = process.env.RTYPE_PANEL_GAME ?? '';

/** Every violation of the CSP the page reports, and every error of its console. */
async function watch(page: Page): Promise<string[]> {
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
  return problems;
}

test('a host opens the panel with its token, watches the game live, ends it, bans and lifts the ban', async ({
  page,
}) => {
  const problems = await watch(page);

  // The token traded for a session: the panel, without the token in its address
  await page.goto(url);
  await expect(page).toHaveURL(/\/$/);
  expect(page.url()).not.toContain('token');
  await expect(page.getByTestId('connection')).toHaveAttribute('data-state', 'open');
  await expect(page.getByRole('heading', { name: 'Overview' })).toBeVisible();

  // The game, live
  await page.getByRole('link', { name: 'Games' }).click();
  const row = page.getByTestId(`game-${gameId}`);
  await expect(row).toContainText('Playwright');
  await expect(row).toContainText('2/4');
  await expect(row).toContainText('Live');
  await row.getByRole('link', { name: 'Playwright' }).click();
  const players = page.getByRole('table', { name: 'Players' });
  await expect(players).toContainText('Alice');
  await expect(players).toContainText('Bob');
  // Measured by the instance once the clients answered
  await expect(players.getByText(/\d+ ms/).first()).toBeVisible({ timeout: 15_000 });

  // Ended, with a reason
  await page.getByRole('button', { name: 'End the game' }).click();
  const dialog = page.getByRole('alertdialog');
  await dialog.getByRole('textbox').fill('end to end');
  await dialog.getByRole('button', { name: 'End the game' }).click();
  await expect(page.getByText('Playwright ended')).toBeVisible();
  await expect(page.getByText(/This game is not running/)).toBeVisible();

  // Its players back in the lobby, and the action recorded
  await page.getByRole('link', { name: 'Players' }).click();
  await expect(page.getByRole('table', { name: 'Players' })).toContainText('In the lobby');
  await page.getByRole('link', { name: 'Actions' }).click();
  const record = page.getByRole('list', { name: 'Actions' });
  await expect(record.locator('[data-action="end_game"]')).toContainText('Playwright');
  await expect(record.locator('[data-action="end_game"]')).toContainText('end to end');

  // Their address banned, which removes them, then the ban lifted
  await page.getByRole('link', { name: 'Players' }).click();
  await page.getByRole('button', { name: 'Ban 127.0.0.1' }).first().click();
  await page.getByRole('alertdialog').getByRole('button', { name: 'Ban' }).click();
  await expect(page.getByText('No player is connected.')).toBeVisible();
  await page.getByRole('link', { name: 'Actions' }).click();
  await page.getByRole('button', { name: 'Lift 127.0.0.1' }).click();
  await page.getByRole('alertdialog').getByRole('textbox').fill('a mistake');
  await page.getByRole('alertdialog').getByRole('button', { name: 'Lift the ban' }).click();
  await expect(page.getByText('No address is banned.')).toBeVisible();
  await expect(record.locator('[data-action="unban"]')).toContainText('ban lifted: a mistake');

  expect(await page.evaluate(() => window.cspViolations)).toEqual([]);
  expect(problems).toEqual([]);
});

test('a page opened without a session says to open admin-url.txt again', async ({ browser }) => {
  const page = await browser.newPage();
  const problems = await watch(page);
  await page.goto(url.split('/?')[0] + '/games');
  await expect(page.getByTestId('connection')).toHaveAttribute('data-state', 'signed-out');
  await expect(page.getByRole('alert')).toContainText('Session expired');
  expect(await page.evaluate(() => window.cspViolations)).toEqual([]);
  // The refusals of the master, 401 to the API and to the WebSocket, are the only errors of its console
  const refusal = (problem: string) => problem.includes('401') || problem.includes('/api/events');
  expect(problems.filter((problem) => !refusal(problem))).toEqual([]);
  await page.close();
});
