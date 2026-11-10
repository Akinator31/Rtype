// The actions on an account (docs/protocol/admin-api.md, 5), each behind its window: ban it with a reason, lift
// its ban, change its role, delete it, mute its voice for a time or lift that mute. The server records each one;
// a toast tells the host.

import { useState } from 'react';

import { api } from '../api/client';
import type { Account, Role } from '../api/types';
import { dayTime, shortDuration } from '../format';
import { strings } from '../strings';
import { useNow } from '../useNow';
import { byteLength, isValidVoiceMuteDuration, MaxTextBytes } from '../validation';
import { DurationField, ReasonField } from './ActionDialogs';
import { ConfirmDialog } from './ConfirmDialog';
import { notify } from './notify';

export type AccountAction = 'ban' | 'unban' | 'promote' | 'demote' | 'delete' | 'mute_voice' | 'unmute_voice';

const fits = (reason: string) => byteLength(reason) <= MaxTextBytes;

/** The window of an action on an account; `onDone` once the master did it. */
export function AccountDialog({
  account,
  action,
  onClose,
  onDone,
}: {
  account: Account;
  action: AccountAction;
  onClose: () => void;
  onDone: () => void;
}) {
  const [reason, setReason] = useState('');
  const [seconds, setSeconds] = useState(86400);
  const [other, setOther] = useState(false);
  const now = useNow();
  const t = strings.dialog;
  const name = `${account.username} #${account.id}`;
  const done = async (work: Promise<unknown>, title: string, detail: string) => {
    await work;
    notify('bonus', title, detail);
    onDone();
  };
  const role = (to: Role) =>
    done(
      api.setRole(account.id, to),
      to === 'admin' ? strings.notify.promoted : strings.notify.demoted,
      name,
    );

  switch (action) {
    case 'ban':
      return (
        <ConfirmDialog
          title={t.banAccountTitle}
          body={t.banAccountBody(name)}
          confirmLabel={t.banAccountConfirm}
          ready={fits(reason)}
          onClose={onClose}
          onConfirm={async () => {
            const banned = await api.banAccount(account.id, reason.trim());
            notify(
              'bonus',
              strings.notify.accountBanned,
              strings.notify.accountBannedDetail(name, banned.removed),
            );
            onDone();
          }}
        >
          <ReasonField value={reason} onChange={setReason} />
        </ConfirmDialog>
      );
    case 'unban':
      return (
        <ConfirmDialog
          title={t.unbanAccountTitle}
          body={t.unbanAccountBody(name)}
          confirmLabel={t.unbanAccountConfirm}
          tone="info"
          ready={fits(reason)}
          onClose={onClose}
          onConfirm={() =>
            done(api.unbanAccount(account.id, reason.trim()), strings.notify.accountUnbanned, name)
          }
        >
          <ReasonField value={reason} onChange={setReason} />
        </ConfirmDialog>
      );
    case 'promote':
      return (
        <ConfirmDialog
          title={t.promoteTitle}
          body={t.promoteBody(name)}
          confirmLabel={strings.accounts.promote}
          tone="info"
          onClose={onClose}
          onConfirm={() => role('admin')}
        />
      );
    case 'demote':
      return (
        <ConfirmDialog
          title={t.demoteTitle}
          body={t.demoteBody(name)}
          confirmLabel={strings.accounts.demote}
          onClose={onClose}
          onConfirm={() => role('player')}
        />
      );
    case 'mute_voice': {
      const valid = isValidVoiceMuteDuration(seconds);
      return (
        <ConfirmDialog
          title={t.muteAccountTitle}
          body={t.muteAccountBody(name)}
          confirmLabel={t.muteAccountConfirm(valid ? shortDuration(seconds) : '')}
          ready={valid && fits(reason)}
          note={valid ? t.muteAccountEnds(dayTime(now + seconds * 1000)) : undefined}
          onClose={onClose}
          onConfirm={async () => {
            const muted = await api.muteAccountVoice(account.id, seconds, reason.trim());
            notify(
              'bonus',
              strings.notify.accountVoiceMuted,
              strings.notify.accountVoiceMutedDetail(name, shortDuration(seconds), muted.removed),
            );
            onDone();
          }}
        >
          <DurationField
            seconds={seconds}
            onChange={setSeconds}
            other={other}
            onOther={setOther}
            choices={t.voiceDurations}
            isValid={isValidVoiceMuteDuration}
            invalidText={t.voiceSecondsInvalid}
          />
          <ReasonField value={reason} onChange={setReason} />
        </ConfirmDialog>
      );
    }
    case 'unmute_voice':
      return (
        <ConfirmDialog
          title={t.unmuteAccountTitle}
          body={t.unmuteAccountBody(name)}
          confirmLabel={t.unmuteAccountConfirm}
          tone="info"
          ready={fits(reason)}
          onClose={onClose}
          onConfirm={() =>
            done(api.unmuteAccountVoice(account.id, reason.trim()), strings.notify.accountVoiceUnmuted, name)
          }
        >
          <ReasonField value={reason} onChange={setReason} />
        </ConfirmDialog>
      );
    case 'delete':
      return (
        <ConfirmDialog
          title={t.deleteTitle}
          body={t.deleteBody(name)}
          confirmLabel={t.deleteConfirm}
          ready={fits(reason)}
          onClose={onClose}
          onConfirm={() =>
            done(api.deleteAccount(account.id, reason.trim()), strings.notify.accountDeleted, name)
          }
        >
          <ReasonField value={reason} onChange={setReason} />
        </ConfirmDialog>
      );
  }
}
