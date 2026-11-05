// The accounts of the server, for a master with accounts (P9′ of the Série 14): each with its Discord picture
// and name, searched by username, email, Discord name or id, newest first; ban one, lift its ban, make it an
// administrator or a player, delete it. The account chosen shows in a sheet beside the table. The
// administrators of the settings are locked: the master refuses those actions on them, and the page does not
// offer them.

import { Flex, Table, Text } from '@chakra-ui/react';
import { useEffect, useState } from 'react';

import { api } from '../api/client';
import type { Account } from '../api/types';
import { notifyFailure } from '../actions';
import { AccountDialog, type AccountAction } from '../components/AccountDialogs';
import { Avatar } from '../components/Avatar';
import { Badge, Button, RowAction } from '../components/Controls';
import { Card, Empty, Pair, SearchField, Toolbar } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { dayTime } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';

/** How long the search waits for the host to stop typing. */
const SearchDelay = 250;

function AccountRow({
  account,
  act,
  chosen,
  choose,
}: {
  account: Account;
  act: (action: AccountAction) => void;
  chosen: boolean;
  choose: () => void;
}) {
  const t = strings.accounts;
  const banned = account.bannedAt !== null;
  return (
    <Table.Row
      h="58px"
      data-testid={`account-${account.id}`}
      aria-selected={chosen}
      cursor="pointer"
      bg={chosen ? 'rgba(255,255,255,0.03)' : undefined}
      onClick={choose}
    >
      <Table.Cell>
        <Flex align="center" gap="10px">
          <Avatar accountId={account.id} name={account.username} discord={account.discord} />
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
      <Table.Cell color="fg.silver">{discordName(account)}</Table.Cell>
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
          <Flex justify="flex-end" gap="10px" onClick={(event) => event.stopPropagation()}>
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

/** The Discord name of an account: `@username`, its id before the profiles were kept, or none. */
function discordName(account: Account): string {
  if (account.discord !== null && account.discord !== undefined && account.discord.username !== '') {
    return `@${account.discord.username}`;
  }
  return account.discordId ?? strings.none;
}

/** The sheet of the account chosen: its profile, what the server keeps of it, and its actions. */
function AccountSheet({ account, act }: { account: Account; act: (action: AccountAction) => void }) {
  const t = strings.accounts;
  const banned = account.bannedAt !== null;
  const shown = account.discord?.displayName ?? null;
  return (
    <Card
      title={t.sheet}
      aside={<Badge tone="neutral">#{account.id}</Badge>}
      w={{ base: '100%', xl: '380px' }}
      flexShrink={0}
    >
      <Flex align="center" gap="16px" py="6px" data-testid="account-sheet">
        <Avatar accountId={account.id} name={account.username} discord={account.discord} size={88} />
        <Flex direction="column" gap="6px" minW={0}>
          <Text textStyle="pageTitle" fontSize="22px" truncate>
            {account.username}
          </Text>
          {(shown !== null || account.discord) && (
            <Text textStyle="body" color="fg.muted" truncate>
              {[shown, account.discord ? `@${account.discord.username}` : null].filter(Boolean).join(' · ')}
            </Text>
          )}
          <Flex gap="8px" wrap="wrap">
            {account.role === 'admin' && <Badge tone="active">{t.admin}</Badge>}
            {account.discord && <Badge tone="neutral">Discord</Badge>}
            {banned && <Badge tone="alert">{t.banned}</Badge>}
          </Flex>
        </Flex>
      </Flex>
      <Pair label={t.email} plain>
        {account.email ?? strings.none}
      </Pair>
      <Pair label={t.verifiedAt} plain>
        {account.emailVerifiedAt === null ? strings.none : dayTime(account.emailVerifiedAt)}
      </Pair>
      <Pair label={t.password} plain>
        {account.password ? t.passwordSet : t.passwordNone}
      </Pair>
      <Pair label={t.discordId} plain>
        {account.discordId ?? strings.none}
      </Pair>
      <Pair label={t.createdAt} plain>
        {dayTime(account.createdAt)}
      </Pair>
      <Pair label={t.lastSeen} plain>
        {dayTime(account.lastSeenAt)}
      </Pair>
      {banned && (
        <Pair label={t.banReason} plain>
          {account.banReason || strings.none}
        </Pair>
      )}
      {account.locked ? (
        <Text textStyle="hint" color="fg.subtle">
          {t.lockedHint}
        </Text>
      ) : (
        <Flex direction="column" gap="10px" pt="8px">
          <Button variant="secondary" onClick={() => act(account.role === 'admin' ? 'demote' : 'promote')}>
            {account.role === 'admin' ? t.demote : t.promote}
          </Button>
          <Button variant={banned ? 'secondary' : 'danger'} onClick={() => act(banned ? 'unban' : 'ban')}>
            {banned ? t.unban : t.ban}
          </Button>
          <Button variant="danger" onClick={() => act('delete')}>
            {t.removeAccount}
          </Button>
        </Flex>
      )}
    </Card>
  );
}

export function Accounts() {
  const live = useLive();
  const [query, setQuery] = useState('');
  const [accounts, setAccounts] = useState<Account[] | null>(null);
  const [open, setOpen] = useState<{ account: Account; action: AccountAction } | null>(null);
  const [chosen, setChosen] = useState<number | null>(null);
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

  // The account chosen, else the first of the list
  const sheet = accounts?.find((account) => account.id === chosen) ?? accounts?.[0];

  return (
    <>
      <Toolbar>
        <SearchField label={t.search} value={query} onChange={setQuery} width="360px" />
      </Toolbar>
      <Flex gap="20px" align="flex-start" direction={{ base: 'column', xl: 'row' }}>
        <Card flex="1" minW={0}>
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
                      chosen={account.id === chosen}
                      choose={() => setChosen(account.id)}
                    />
                  ))}
                </Table.Body>
              </Table.Root>
            </Table.ScrollArea>
          )}
        </Card>
        {sheet !== undefined && (
          <AccountSheet account={sheet} act={(action) => setOpen({ account: sheet, action })} />
        )}
      </Flex>
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
