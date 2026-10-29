// The actions of the administration (docs/protocol/admin-api.md, 4), each behind its dialog: remove a player,
// ban an address, end a game, send a message. The server records each one; the panel tells the host.

import { Field, Input, NativeSelect } from '@chakra-ui/react';
import { useState } from 'react';

import { api } from '../api/client';
import { strings } from '../strings';
import { byteLength, isValidMessage, MaxTextBytes } from '../validation';
import { ConfirmDialog } from './ConfirmDialog';
import { notify } from './notify';

function ReasonField({ value, onChange }: { value: string; onChange: (value: string) => void }) {
  const tooLong = byteLength(value) > MaxTextBytes;
  return (
    <Field.Root invalid={tooLong}>
      <Field.Label textStyle="caption" color="fg.muted">
        {strings.dialog.reason}
      </Field.Label>
      <Input value={value} onChange={(event) => onChange(event.target.value)} autoComplete="off" />
      <Field.HelperText color="fg.subtle">{strings.dialog.reasonHint}</Field.HelperText>
    </Field.Root>
  );
}

export function KickDialog({
  player,
  onClose,
}: {
  player: { id: number; name: string };
  onClose: () => void;
}) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.kickTitle(player.name)}
      body={strings.dialog.kickBody}
      confirmLabel={strings.players.remove}
      ready={byteLength(reason) <= MaxTextBytes}
      onClose={onClose}
      onConfirm={async () => {
        await api.kick(player.id, reason.trim());
        notify('alert', strings.notify.kicked(player.name), reason.trim() || undefined);
      }}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

export function BanDialog({ address, onClose }: { address: string; onClose: () => void }) {
  const [reason, setReason] = useState('');
  const [duration, setDuration] = useState(strings.dialog.durations[0]?.seconds ?? 3600);
  return (
    <ConfirmDialog
      title={strings.dialog.banTitle(address)}
      body={strings.dialog.banBody}
      confirmLabel={strings.players.ban}
      ready={byteLength(reason) <= MaxTextBytes}
      onClose={onClose}
      onConfirm={async () => {
        const banned = await api.ban(address, duration, reason.trim());
        notify('alert', strings.notify.banned(address), strings.notify.bannedDetail(banned.removed));
      }}
    >
      <Field.Root>
        <Field.Label textStyle="caption" color="fg.muted">
          {strings.dialog.duration}
        </Field.Label>
        <NativeSelect.Root>
          <NativeSelect.Field value={duration} onChange={(event) => setDuration(Number(event.target.value))}>
            {strings.dialog.durations.map((choice) => (
              <option key={choice.seconds} value={choice.seconds}>
                {choice.label}
              </option>
            ))}
          </NativeSelect.Field>
          <NativeSelect.Indicator />
        </NativeSelect.Root>
      </Field.Root>
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

export function UnbanDialog({ address, onClose }: { address: string; onClose: () => void }) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.unbanTitle(address)}
      body={strings.dialog.unbanBody}
      confirmLabel={strings.dialog.unbanConfirm}
      tone="light"
      ready={byteLength(reason) <= MaxTextBytes}
      onClose={onClose}
      onConfirm={async () => {
        await api.unban(address, reason.trim());
        notify('bonus', strings.notify.unbanned(address), reason.trim() || undefined);
      }}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

export function EndGameDialog({
  game,
  onClose,
}: {
  game: { id: number; name: string };
  onClose: () => void;
}) {
  const [reason, setReason] = useState('');
  return (
    <ConfirmDialog
      title={strings.dialog.endTitle(game.name)}
      body={strings.dialog.endBody}
      confirmLabel={strings.game.end}
      ready={byteLength(reason) <= MaxTextBytes}
      onClose={onClose}
      onConfirm={async () => {
        await api.endGame(game.id, reason.trim());
        notify('alert', strings.notify.ended(game.name), reason.trim() || undefined);
      }}
    >
      <ReasonField value={reason} onChange={setReason} />
    </ConfirmDialog>
  );
}

/** A message to a game, or to every player when `game` is null. */
export function MessageDialog({
  game,
  onClose,
}: {
  game: { id: number; name: string } | null;
  onClose: () => void;
}) {
  const [text, setText] = useState('');
  const valid = isValidMessage(text);
  return (
    <ConfirmDialog
      title={strings.dialog.messageTitle(game?.name ?? strings.dialog.everyone)}
      body={strings.dialog.messageBody}
      confirmLabel={strings.dialog.confirm}
      tone="light"
      ready={valid}
      onClose={onClose}
      onConfirm={async () => {
        const sent = await api.message(text.trim(), game?.id ?? null);
        notify('bonus', strings.notify.sent, strings.notify.sentDetail(sent.recipients));
      }}
    >
      <Field.Root invalid={text !== '' && !valid}>
        <Field.Label textStyle="caption" color="fg.muted">
          {strings.dialog.text}
        </Field.Label>
        <Input value={text} onChange={(event) => setText(event.target.value)} autoComplete="off" autoFocus />
        <Field.HelperText color="fg.subtle">{strings.dialog.textHint}</Field.HelperText>
      </Field.Root>
    </ConfirmDialog>
  );
}
