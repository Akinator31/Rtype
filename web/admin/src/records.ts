// How the panel shows an action of the record (docs/protocol/admin-api.md, 3): its badge, its target, its
// details on a line; and how it finds a game, and the bans that ended.

import type { Action, Ban, Game } from './api/types';
import type { BadgeTone } from './components/Controls';
import { dayTime, time } from './format';
import { strings } from './strings';

const text = (value: unknown): string | undefined =>
  typeof value === 'string' && value !== '' ? value : undefined;
const number = (value: unknown): number | undefined => (typeof value === 'number' ? value : undefined);

/** The badge of an action: red for a ban and an end, silver for the rest, muted for a ban lifted. */
export function actionBadge(action: Action): { label: string; tone: BadgeTone } {
  const label = strings.actionKinds[action.action] ?? action.action;
  switch (action.action) {
    case 'ban':
    case 'end_game':
      return { label, tone: 'alert' };
    case 'unban':
      return { label, tone: 'muted' };
    default:
      return { label, tone: 'neutral' };
  }
}

/** The target for a person: the name of the player or of the game, the address, everyone. */
export function targetLabel(action: Action): string {
  const name = text(action.details.name);
  const id = /:(\d+)$/.exec(action.target)?.[1];
  if (action.target === 'everyone') return strings.messages.everyone;
  if (action.target.startsWith('address:')) return action.target.replace(/^address:/, '');
  if (name !== undefined && id !== undefined) return `${name} #${id}`;
  return action.target.replace(/^game:/, 'game #').replace(/^player:/, 'player #');
}

/** The details of an action on one line, as the record of the design shows them. */
export function detailsLine(action: Action): string {
  const details = action.details;
  const reason = text(details.reason);
  const words = strings.actionDetails;
  const parts: (string | undefined)[] = [];
  switch (action.action) {
    case 'kick':
      parts.push(
        text(details.name),
        text(details.address),
        number(details.gameId) !== undefined ? `game #${String(details.gameId)}` : undefined,
        reason,
      );
      break;
    case 'end_game':
      parts.push(
        text(details.name),
        Array.isArray(details.players) && details.players.length > 0 ? details.players.join(', ') : undefined,
        reason,
      );
      break;
    case 'message':
      parts.push(
        text(details.text) !== undefined ? `“${String(details.text)}”` : undefined,
        number(details.recipients) !== undefined ? words.recipients(details.recipients as number) : undefined,
      );
      break;
    case 'ban':
      parts.push(
        number(details.duration) !== undefined ? words.seconds(details.duration as number) : undefined,
        number(details.expiresAt) !== undefined
          ? words.endsAt(dayTime(details.expiresAt as number))
          : undefined,
        reason,
        number(details.removed) !== undefined ? words.removed(details.removed as number) : undefined,
      );
      break;
    case 'unban':
      parts.push(
        number(details.expiresAt) !== undefined
          ? words.plannedEnd(dayTime(details.expiresAt as number))
          : undefined,
        text(details.bannedBy) !== undefined ? words.bannedBy(String(details.bannedBy)) : undefined,
        reason,
      );
      break;
    default:
      parts.push(reason);
  }
  return parts.filter((part) => part !== undefined).join(' · ');
}

/** A short line for the overview: the reason, or the text, then the time. */
export function summaryLine(action: Action): string {
  const details = action.details;
  const first = action.action === 'message' ? text(details.text) : text(details.reason);
  return [first, time(action.at)].filter((part) => part !== undefined).join(' · ');
}

/** The actions of a list, each once, the first given first: the live ones, then those read. */
export function merged(...lists: Action[][]): Action[] {
  const seen = new Set<number>();
  return lists.flat().filter((action) => {
    if (seen.has(action.id)) return false;
    seen.add(action.id);
    return true;
  });
}

/** The games for this search, by name or id. */
export function matchesGame(game: Game, query: string): boolean {
  const needle = query.trim().toLowerCase().replace(/^#/, '');
  return needle === '' || game.name.toLowerCase().includes(needle) || String(game.id) === needle;
}

/** The game as an action takes it: its id, its name, its players. */
export const gameRef = (game: Game) => ({ id: game.id, name: game.name, players: game.players.length });

interface Ended {
  key: number;
  address: string;
  at: number;
  lifted: boolean;
  reason: string;
  actor: string;
}

/** The bans that ended, newest first: lifted by an action, or past their end and not banned again. */
export function endedBans(actions: Action[], current: Ban[], now: number): Ended[] {
  const banned = new Set(current.map((ban) => ban.address));
  const seen = new Set<string>();
  const ended: Ended[] = [];
  for (const action of actions) {
    const address = action.target.replace(/^address:/, '');
    if (seen.has(address) || (action.action !== 'ban' && action.action !== 'unban')) continue;
    seen.add(address);
    const reason = typeof action.details.reason === 'string' ? action.details.reason : '';
    if (action.action === 'unban') {
      ended.push({ key: action.id, address, at: action.at, lifted: true, reason, actor: action.actor });
    } else if (
      typeof action.details.expiresAt === 'number' &&
      action.details.expiresAt <= now &&
      !banned.has(address)
    ) {
      ended.push({
        key: action.id,
        address,
        at: action.details.expiresAt,
        lifted: false,
        reason,
        actor: action.actor,
      });
    }
  }
  return ended.sort((a, b) => b.at - a.at).slice(0, 8);
}
