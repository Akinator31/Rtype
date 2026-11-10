// The actions of a page, each behind its window. Removing, ending and muting a pilot act at once when the host
// chose not to confirm them in the settings; a ban always asks its duration.

import { useState, type ReactNode } from 'react';

import { end, kick, muteVoice, notifyFailure, type GameRef, type PlayerRef } from '../actions';
import { preferences } from '../preferences';
import { BanDialog, EndGameDialog, KickDialog, UnbanDialog, VoiceDialog } from './ActionDialogs';

type Open =
  | { kind: 'kick'; player: PlayerRef }
  | { kind: 'end'; game: GameRef }
  | { kind: 'ban'; address: string }
  | { kind: 'unban'; address: string }
  | { kind: 'voice'; player: PlayerRef; muted: boolean };

/** Opens the window of an action, or acts; `dialogs` is where the window shows. */
export function useActions(): {
  kick: (player: PlayerRef) => void;
  end: (game: GameRef) => void;
  ban: (address: string) => void;
  unban: (address: string) => void;
  /** Mutes a pilot for the rest of its game, or lets it talk again. */
  voice: (player: PlayerRef, muted: boolean) => void;
  dialogs: ReactNode;
} {
  const [open, setOpen] = useState<Open | null>(null);
  const close = () => setOpen(null);
  const confirm = preferences().confirmActions;
  return {
    kick: (player) =>
      confirm ? setOpen({ kind: 'kick', player }) : void kick(player, '').catch(notifyFailure),
    end: (game) => (confirm ? setOpen({ kind: 'end', game }) : void end(game, '').catch(notifyFailure)),
    ban: (address) => setOpen({ kind: 'ban', address }),
    unban: (address) => setOpen({ kind: 'unban', address }),
    voice: (player, muted) =>
      confirm
        ? setOpen({ kind: 'voice', player, muted })
        : void muteVoice(player, muted, '').catch(notifyFailure),
    dialogs: (
      <>
        {open?.kind === 'kick' && <KickDialog player={open.player} onClose={close} />}
        {open?.kind === 'end' && <EndGameDialog game={open.game} onClose={close} />}
        {open?.kind === 'ban' && <BanDialog address={open.address} onClose={close} />}
        {open?.kind === 'unban' && <UnbanDialog address={open.address} onClose={close} />}
        {open?.kind === 'voice' && <VoiceDialog player={open.player} muted={open.muted} onClose={close} />}
      </>
    ),
  };
}
