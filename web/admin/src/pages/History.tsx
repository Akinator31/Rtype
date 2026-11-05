// The record of the actions (node 149:574): searched and filtered by kind, a page at a time, newest first, the
// actions of the WebSocket on the first page; an entry unfolded shows its details as the master keeps them.

import { Flex, Table, Text } from '@chakra-ui/react';
import { Fragment, useCallback, useEffect, useMemo, useState } from 'react';

import { ApiError, api } from '../api/client';
import type { Action } from '../api/types';
import { Badge, Button, Chip, RowAction } from '../components/Controls';
import { Card, Empty, SearchField, Toolbar } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { dateTime, dayTime } from '../format';
import { useLive } from '../live/context';
import { actionBadge, detailsLine, merged, targetLabel } from '../records';
import { strings } from '../strings';
import { glow } from '../theme/system';

export const PageSize = 100;

const kinds = [
  'kick',
  'end_game',
  'message',
  'ban',
  'unban',
  'ban_account',
  'unban_account',
  'role',
  'delete_account',
] as const;

/** A value of the details, for a person: the times as dates, the lists joined, nothing as a dash. */
function readable(key: string, value: unknown): string {
  if (typeof value === 'number' && key.endsWith('At')) return dateTime(value);
  if (Array.isArray(value)) return value.join(', ');
  if (value === '' || value === null || value === undefined) return strings.none;
  return String(value);
}

/** The details of an entry, as the master keeps them, under its line. */
function Details({ action }: { action: Action }) {
  const fields: [string, unknown][] = [
    ['id', action.id],
    ['actor', action.actor],
    ['target', action.target],
    ...Object.entries(action.details),
  ];
  return (
    <Flex gap="40px" wrap="wrap" py="16px" data-testid="entry-details">
      {fields.map(([key, value]) => (
        <Flex key={key} direction="column" gap="8px" minW={0}>
          <Text fontSize="10px" letterSpacing="1px" color="fg.subtle">
            {key}
          </Text>
          <Text textStyle="number" color="fg" textShadow={glow(0.35)} textTransform="none">
            {readable(key, value)}
          </Text>
        </Flex>
      ))}
    </Flex>
  );
}

export function History() {
  const live = useLive();
  const [pages, setPages] = useState<number[]>([]);
  const [loaded, setLoaded] = useState<Action[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [query, setQuery] = useState('');
  const [kind, setKind] = useState<string | null>(null);
  const [open, setOpen] = useState<number | null>(null);
  const t = strings.history;
  usePageTitle(t.title);
  const before = pages[pages.length - 1];

  const load = useCallback((page: number | undefined) => {
    let current = true;
    api
      .actions({ limit: PageSize, before: page })
      .then((answer) => {
        if (!current) return;
        setLoaded(answer);
        setError(null);
      })
      .catch(
        (failure: unknown) =>
          current && setError(failure instanceof ApiError ? failure.message : String(failure)),
      );
    return () => {
      current = false;
    };
  }, []);
  useEffect(() => load(before), [load, before]);

  // The first page takes the actions of the WebSocket too, newest first
  const actions = useMemo(
    () => (before === undefined ? merged(live.actions, loaded) : loaded).slice(0, PageSize),
    [before, live.actions, loaded],
  );
  const shown = useMemo(() => {
    const needle = query.trim().toLowerCase();
    return actions.filter(
      (action) =>
        (kind === null || action.action === kind) &&
        (needle === '' ||
          action.target.toLowerCase().includes(needle) ||
          detailsLine(action).toLowerCase().includes(needle)),
    );
  }, [actions, query, kind]);
  const oldest = actions[actions.length - 1];

  return (
    <>
      <Toolbar>
        <SearchField label={t.search} value={query} onChange={setQuery} width="260px" />
        <Flex gap="12px" wrap="wrap" role="group" aria-label={t.action}>
          <Chip pressed={kind === null} onClick={() => setKind(null)}>
            {t.all}
          </Chip>
          {kinds.map((which) => (
            <Chip key={which} pressed={kind === which} onClick={() => setKind(which)}>
              {strings.actionKinds[which]}
            </Chip>
          ))}
        </Flex>
      </Toolbar>
      <Card>
        {error !== null && (
          <Text role="alert" textStyle="hint" color="alert">
            {error}
          </Text>
        )}
        {shown.length === 0 ? (
          <Empty>{actions.length === 0 ? t.empty : t.noMatch}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.journal}>
              <Table.Header>
                <Table.Row>
                  <Table.ColumnHeader>{t.time}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.action}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.target}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.details}</Table.ColumnHeader>
                  <Table.ColumnHeader />
                </Table.Row>
              </Table.Header>
              <Table.Body>
                {shown.map((action) => {
                  const badge = actionBadge(action);
                  const unfolded = action.id === open;
                  const toggle = () => setOpen(unfolded ? null : action.id);
                  return (
                    <Fragment key={action.id}>
                      <Table.Row
                        data-action={action.action}
                        aria-selected={unfolded}
                        cursor="pointer"
                        onClick={toggle}
                      >
                        <Table.Cell whiteSpace="nowrap">{dayTime(action.at)}</Table.Cell>
                        <Table.Cell>
                          <Badge tone={badge.tone}>{badge.label}</Badge>
                        </Table.Cell>
                        <Table.Cell textStyle="name" textShadow={glow(0.3)}>
                          {targetLabel(action)}
                        </Table.Cell>
                        <Table.Cell color="fg.silver" maxW="520px" truncate>
                          {detailsLine(action)}
                        </Table.Cell>
                        <Table.Cell textAlign="right">
                          <RowAction
                            onClick={(event) => {
                              event.stopPropagation();
                              toggle();
                            }}
                            aria-expanded={unfolded}
                            aria-label={`${unfolded ? t.fold : t.expand} ${action.id}`}
                          >
                            {unfolded ? t.fold : t.expand}
                          </RowAction>
                        </Table.Cell>
                      </Table.Row>
                      {unfolded && (
                        <Table.Row _hover={{ bgImage: 'none' }}>
                          <Table.Cell colSpan={5} h="auto">
                            <Details action={action} />
                          </Table.Cell>
                        </Table.Row>
                      )}
                    </Fragment>
                  );
                })}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Card>
      {(pages.length > 0 || loaded.length === PageSize) && (
        <Flex justify="flex-end" gap="12px">
          {pages.length > 0 && (
            <Button onClick={() => setPages((current) => current.slice(0, -1))}>{t.newer}</Button>
          )}
          {oldest !== undefined && loaded.length === PageSize && (
            <Button onClick={() => setPages((current) => [...current, oldest.id])}>{t.older}</Button>
          )}
        </Flex>
      )}
    </>
  );
}
