// The actions on an account (docs/protocol/admin-api.md, 5), each behind its window: ban it with a reason, lift
// its ban, change its role, delete it. The server records each one; a toast tells the host.

import { useState } from 'react';

import { api } from '../api/client';
import type { Account, Role } from '../api/types';
import { strings } from '../strings';
import { byteLength, MaxTextBytes } from '../validation';
import { ReasonField } from './ActionDialogs';
import { ConfirmDialog } from './ConfirmDialog';
import { notify } from './notify';

export type AccountAction = 'ban' | 'unban' | 'promote' | 'demote' | 'delete';

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
