// The accounts of the server, for a master with accounts: searched by username, email or Discord id, newest
// first; ban one, lift its ban, make it an administrator or a player, delete it. The administrators of the
// settings are locked: the master refuses those actions on them, and the page does not offer them.

import { Flex, Table, Text } from '@chakra-ui/react';
import { useEffect, useState } from 'react';

import { api } from '../api/client';
import type { Account } from '../api/types';
import { notifyFailure } from '../actions';
import { AccountDialog, type AccountAction } from '../components/AccountDialogs';
import { Badge, RowAction } from '../components/Controls';
import { Card, Empty, SearchField, Toolbar } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { dayTime } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';

/** How long the search waits for the host to stop typing. */
const SearchDelay = 250;

function AccountRow({ account, act }: { account: Account; act: (action: AccountAction) => void }) {
  const t = strings.accounts;
  const banned = account.bannedAt !== null;
  return (
    <Table.Row h="58px" data-testid={`account-${account.id}`}>
      <Table.Cell>
        <Flex align="center" gap="10px">
          <Text textStyle="name">{account.username}</Text>
          {account.locked && (
            <Badge tone="active" title={t.lockedHint}>
              {t.locked}
            </Badge>
          )}
        </Flex>
      </Table.Cell>
      <Table.Cell color="fg.silver">
        {account.email ?? strings.none}
        {account.email !== null && account.emailVerifiedAt === null && (
          <Text as="span" textStyle="hint" color="fg.subtle">
            {' '}
            · {t.notVerified}
          </Text>
        )}
      </Table.Cell>
      <Table.Cell textStyle="number">{account.discordId ?? strings.none}</Table.Cell>
      <Table.Cell>
        <Badge tone={account.role === 'admin' ? 'active' : 'neutral'}>
          {account.role === 'admin' ? t.admin : t.player}
        </Badge>
      </Table.Cell>
      <Table.Cell>
        {banned ? (
          <Badge tone="alert" title={account.banReason || undefined}>
            {t.banned}
          </Badge>
        ) : (
          <Badge tone="muted">{t.active}</Badge>
        )}
      </Table.Cell>
      <Table.Cell whiteSpace="nowrap">{dayTime(account.lastSeenAt)}</Table.Cell>
      <Table.Cell>
        {!account.locked && (
          <Flex justify="flex-end" gap="10px">
            {account.role === 'admin' ? (
              <RowAction onClick={() => act('demote')} aria-label={`${t.demote} ${account.username}`}>
                {t.demote}
              </RowAction>
            ) : (
              <RowAction onClick={() => act('promote')} aria-label={`${t.promote} ${account.username}`}>
                {t.promote}
              </RowAction>
            )}
            {banned ? (
              <RowAction onClick={() => act('unban')} aria-label={`${t.unban} ${account.username}`}>
                {t.unban}
              </RowAction>
            ) : (
              <RowAction tone="alert" onClick={() => act('ban')} aria-label={`${t.ban} ${account.username}`}>
                {t.ban}
              </RowAction>
            )}
            <RowAction
              tone="alert"
              onClick={() => act('delete')}
              aria-label={`${t.remove} ${account.username}`}
            >
              {t.remove}
            </RowAction>
          </Flex>
        )}
      </Table.Cell>
    </Table.Row>
  );
}

export function Accounts() {
  const live = useLive();
  const [query, setQuery] = useState('');
  const [accounts, setAccounts] = useState<Account[] | null>(null);
  const [open, setOpen] = useState<{ account: Account; action: AccountAction } | null>(null);
  const [changes, setChanges] = useState(0);
  const t = strings.accounts;
  const latest = live.actions[0]?.id;
  usePageTitle(t.title);

  // Asked again when the search changes, after an action of this page, and after each action recorded
  useEffect(() => {
    let current = true;
    const timer = setTimeout(() => {
      api
        .accounts({ search: query.trim() })
        .then((answer) => current && setAccounts(answer))
        .catch((failure: unknown) => current && notifyFailure(failure));
    }, SearchDelay);
    return () => {
      current = false;
      clearTimeout(timer);
    };
  }, [query, changes, latest]);

  return (
    <>
      <Toolbar>
        <SearchField label={t.search} value={query} onChange={setQuery} width="360px" />
      </Toolbar>
      <Card>
        {accounts !== null && accounts.length === 0 ? (
          <Empty>{query.trim() === '' ? t.empty : t.noMatch}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.table}>
              <Table.Header>
                <Table.Row>
                  <Table.ColumnHeader>{t.username}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.email}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.discord}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.role}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.state}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.lastSeen}</Table.ColumnHeader>
                  <Table.ColumnHeader />
                </Table.Row>
              </Table.Header>
              <Table.Body>
                {(accounts ?? []).map((account) => (
                  <AccountRow
                    key={account.id}
                    account={account}
                    act={(action) => setOpen({ account, action })}
                  />
                ))}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Card>
      {open !== null && (
        <AccountDialog
          account={open.account}
          action={open.action}
          onClose={() => setOpen(null)}
          onDone={() => setChanges((count) => count + 1)}
        />
      )}
    </>
  );
}
