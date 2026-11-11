// The actions of the administration as the pages do them (docs/protocol/admin-api.md, 4): sent to the master,
// then told to the host by a toast, its success or the error of the master.

import { ApiError, api } from './api/client';
import { notify } from './components/notify';
import { shortDuration } from './format';
import { strings } from './strings';

export interface PlayerRef {
  id: number;
  name: string;
}

export interface GameRef {
  id: number;
  name: string;
  players: number;
}

const withReason = (subject: string, reason: string) =>
  reason.trim() ? `${subject} · ${reason.trim()}` : subject;

/** The failure of an action, as a toast: its status and the error of the master. */
export function notifyFailure(failure: unknown): void {
  if (failure instanceof ApiError) {
    notify('alert', strings.notify.failed(failure.status), failure.message);
  } else {
    notify('alert', strings.notify.failed(0), String(failure));
  }
}

export async function kick(player: PlayerRef, reason: string): Promise<void> {
  await api.kick(player.id, reason.trim());
  notify('bonus', strings.notify.kicked, withReason(`${player.name} #${player.id}`, reason));
}

/** Mutes a pilot for the rest of its game, or lets it talk again. */
export async function muteVoice(player: PlayerRef, muted: boolean, reason: string): Promise<void> {
  await (muted ? api.muteVoice : api.unmuteVoice)(player.id, reason.trim());
  notify(
    'bonus',
    muted ? strings.notify.voiceMuted : strings.notify.voiceUnmuted,
    withReason(`${player.name} #${player.id}`, reason),
  );
}

export async function end(game: GameRef, reason: string): Promise<void> {
  await api.endGame(game.id, reason.trim());
  notify('bonus', strings.notify.ended, withReason(`${game.name} #${game.id}`, reason));
}

export async function ban(address: string, seconds: number, reason: string): Promise<void> {
  const banned = await api.ban(address, seconds, reason.trim());
  notify(
    'bonus',
    strings.notify.banned,
    strings.notify.bannedDetail(address, shortDuration(seconds), banned.removed),
  );
}
