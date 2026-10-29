// The record of the actions, after the design's feed of events (N6): one line per action, newest first,
// fading with age; the older ones a page at a time. Then the addresses banned now.

import { Flex, Table, Text } from '@chakra-ui/react';
import { useCallback, useEffect, useMemo, useState } from 'react';

import { ApiError, api } from '../api/client';
import type { Action, Ban } from '../api/types';
import { UnbanDialog } from '../components/ActionDialogs';
import { EventLine } from '../components/EventFeed';
import { FlareButton } from '../components/Flare';
import { Empty, PageHeader, Surface } from '../components/Layout';
import { clock, dateTime, fadeByAge } from '../format';
import { useLive } from '../live/context';
import { describeAction } from '../live/state';
import { strings } from '../strings';
import { useNow } from '../useNow';

export const PageSize = 50;

export function Actions() {
  const live = useLive();
  const now = useNow(10_000);
  const [loaded, setLoaded] = useState<Action[]>([]);
  const [more, setMore] = useState(false);
  const [bans, setBans] = useState<Ban[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [lifting, setLifting] = useState<string | null>(null);
  const t = strings.actions;
  const latest = live.actions[0]?.id;

  const take = useCallback((page: Action[], older: boolean) => {
    setLoaded((current) => (older ? [...current, ...page] : page));
    setMore(page.length === PageSize);
    setError(null);
  }, []);
  const fail = useCallback((failure: unknown) => {
    setError(failure instanceof ApiError ? failure.message : String(failure));
  }, []);

  useEffect(() => {
    let current = true;
    api
      .actions({ limit: PageSize })
      .then((page) => current && take(page, false))
      .catch((failure: unknown) => current && fail(failure));
    return () => {
      current = false;
    };
  }, [take, fail]);

  const older = (before: number) => {
    api
      .actions({ limit: PageSize, before })
      .then((page) => take(page, true))
      .catch(fail);
  };

  // The bans change with the actions
  useEffect(() => {
    let current = true;
    api
      .bans()
      .then((answer) => current && setBans(answer))
      .catch(() => undefined);
    return () => {
      current = false;
    };
  }, [latest]);

  // The actions of the WebSocket first, then those asked, each once
  const actions = useMemo(() => {
    const seen = new Set<number>();
    return [...live.actions, ...loaded].filter((action) => {
      if (seen.has(action.id)) return false;
      seen.add(action.id);
      return true;
    });
  }, [live.actions, loaded]);

  const last = actions[actions.length - 1];
  return (
    <>
      <PageHeader caption={t.caption} title={t.title} description={t.description} />
      <Surface mb="16px">
        {error !== null && (
          <Text role="alert" color="alert" mb="16px">
            {error}
          </Text>
        )}
        {actions.length === 0 ? (
          <Empty>{t.empty}</Empty>
        ) : (
          <Flex direction="column" gap="10px" role="list" aria-label={t.title}>
            {actions.map((action) => (
              <div role="listitem" key={action.id} data-action={action.action}>
                <EventLine
                  {...describeAction(action)}
                  opacity={fadeByAge(now - action.at)}
                  lead={
                    <Text
                      as="span"
                      textStyle="number"
                      fontSize="10px"
                      color="fg.subtle"
                      title={dateTime(action.at)}
                    >
                      {clock(action.at)}
                    </Text>
                  }
                />
              </div>
            ))}
          </Flex>
        )}
        {more && last !== undefined && (
          <Flex justify="center" mt="16px">
            <FlareButton onClick={() => older(last.id)}>{t.more}</FlareButton>
          </Flex>
        )}
      </Surface>
      <Surface title={t.bans}>
        {bans.length === 0 ? (
          <Empty>{t.noBans}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.bans}>
              <Table.Body>
                {bans.map((ban) => (
                  <Table.Row key={ban.address}>
                    <Table.Cell>{ban.address}</Table.Cell>
                    <Table.Cell color="fg.muted">{ban.reason}</Table.Cell>
                    <Table.Cell textAlign="right" color="fg.muted">
                      {t.expires} {dateTime(ban.expiresAt)}
                    </Table.Cell>
                    <Table.Cell textAlign="right">
                      <FlareButton
                        size="sm"
                        onClick={() => setLifting(ban.address)}
                        aria-label={`${t.lift} ${ban.address}`}
                      >
                        {t.lift}
                      </FlareButton>
                    </Table.Cell>
                  </Table.Row>
                ))}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Surface>
      {lifting !== null && <UnbanDialog address={lifting} onClose={() => setLifting(null)} />}
    </>
  );
}
