// The actions of a page, each behind its window. Removing and ending act at once when the host chose not to
// confirm them in the settings; a ban always asks its duration.

import { useState, type ReactNode } from 'react';

import { end, kick, notifyFailure, type GameRef, type PlayerRef } from '../actions';
import { preferences } from '../preferences';
import { BanDialog, EndGameDialog, KickDialog, UnbanDialog } from './ActionDialogs';

type Open =
  | { kind: 'kick'; player: PlayerRef }
  | { kind: 'end'; game: GameRef }
  | { kind: 'ban'; address: string }
  | { kind: 'unban'; address: string };

/** Opens the window of an action, or acts; `dialogs` is where the window shows. */
export function useActions(): {
  kick: (player: PlayerRef) => void;
  end: (game: GameRef) => void;
  ban: (address: string) => void;
  unban: (address: string) => void;
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
    dialogs: (
      <>
        {open?.kind === 'kick' && <KickDialog player={open.player} onClose={close} />}
        {open?.kind === 'end' && <EndGameDialog game={open.game} onClose={close} />}
        {open?.kind === 'ban' && <BanDialog address={open.address} onClose={close} />}
        {open?.kind === 'unban' && <UnbanDialog address={open.address} onClose={close} />}
      </>
    ),
  };
}
