// The running games, live (node 147:110): an instance stuck on top, the search and the filters by state, each
// line updated by the status its instance sends every second. A line opens its game.

import { Flex, Table, Text } from '@chakra-ui/react';
import { useMemo, useState } from 'react';
import { Link, useNavigate } from 'react-router';

import type { Game } from '../api/types';
import { AlertStrip } from '../components/AlertStrip';
import { Button, Chip, RowAction } from '../components/Controls';
import { Card, Empty, SearchField, Toolbar } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { useActions } from '../components/useActions';
import { duration, tenths, wave } from '../format';
import { useLive } from '../live/context';
import { gameRef, matchesGame } from '../records';
import { strings } from '../strings';
import { alertGlow } from '../theme/system';
import { useNow } from '../useNow';
import { GameState } from './Overview';

type Filter = 'all' | 'running' | 'stuck' | 'waiting';

const filters: Record<Filter, (game: Game) => boolean> = {
  all: () => true,
  running: (game) => !game.stale && game.tick !== null,
  stuck: (game) => game.stale,
  waiting: (game) => !game.stale && game.tick === null,
};

export function Games() {
  const { games } = useLive();
  const navigate = useNavigate();
  const now = useNow();
  const actions = useActions();
  const [query, setQuery] = useState('');
  const [filter, setFilter] = useState<Filter>('all');
  const t = strings.games;
  usePageTitle(t.title);

  const shown = useMemo(
    () => games.filter((game) => filters[filter](game) && matchesGame(game, query)),
    [games, query, filter],
  );
  const stuck = games.find((game) => game.stale);
  const count = (which: Filter) => games.filter(filters[which]).length;

  return (
    <>
      {stuck !== undefined && (
        <AlertStrip
          title={t.stuckTitle}
          detail={t.stuckDetail(`${stuck.name} #${stuck.id}`, tenths(stuck.statusAge))}
          action={
            <Button variant="danger" onClick={() => actions.end(gameRef(stuck))}>
              {t.endStuck(stuck.name)}
            </Button>
          }
        />
      )}
      <Toolbar>
        <SearchField label={t.search} value={query} onChange={setQuery} />
        <Flex gap="12px" wrap="wrap" role="group" aria-label={t.state}>
          {(['all', 'running', 'stuck', 'waiting'] as const).map((which) => (
            <Chip
              key={which}
              pressed={filter === which}
              tone={which === 'stuck' && count('stuck') > 0 ? 'alert' : 'neutral'}
              onClick={() => setFilter(which)}
            >
              {t[which]} · {count(which)}
            </Chip>
          ))}
        </Flex>
      </Toolbar>
      <Card>
        {shown.length === 0 ? (
          <Empty>{games.length === 0 ? t.empty : t.noMatch}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.table}>
              <Table.Header>
                <Table.Row>
                  <Table.ColumnHeader>{t.name}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.seats}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.wave}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.playing}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.state}</Table.ColumnHeader>
                  <Table.ColumnHeader />
                </Table.Row>
              </Table.Header>
              <Table.Body>
                {shown.map((game) => (
                  <Table.Row
                    key={game.id}
                    data-testid={`game-${game.id}`}
                    data-tone={game.stale ? 'alert' : undefined}
                    cursor="pointer"
                    onClick={() => void navigate(`/games/${game.id}`)}
                  >
                    <Table.Cell>
                      <Flex align="baseline" gap="8px">
                        <Text
                          asChild
                          textStyle="name"
                          textDecoration="none"
                          {...(game.stale ? { color: 'alert', textShadow: alertGlow(0.5) } : {})}
                        >
                          <Link to={`/games/${game.id}`} onClick={(event) => event.stopPropagation()}>
                            {game.name}
                          </Link>
                        </Text>
                        <Text as="span" textStyle="number" color="fg.subtle">
                          #{game.id}
                        </Text>
                      </Flex>
                    </Table.Cell>
                    <Table.Cell textStyle="number" color="fg">
                      {t.seatsValue(game.players.length, game.maxPlayers)}
                    </Table.Cell>
                    <Table.Cell textStyle="number" color="fg">
                      {game.wave === null ? strings.none : wave(game.wave)}
                    </Table.Cell>
                    <Table.Cell whiteSpace="nowrap">{duration(now - game.createdAt)}</Table.Cell>
                    <Table.Cell>
                      <GameState game={game} />
                    </Table.Cell>
                    <Table.Cell>
                      <Flex justify="flex-end" gap="12px" onClick={(event) => event.stopPropagation()}>
                        {!game.stale && (
                          <RowAction
                            onClick={() => void navigate(`/messages?to=${game.id}`)}
                            aria-label={`${t.message} ${game.name}`}
                          >
                            {t.message}
                          </RowAction>
                        )}
                        <RowAction
                          tone="alert"
                          onClick={() => actions.end(gameRef(game))}
                          aria-label={`${t.end} ${game.name}`}
                        >
                          {t.end}
                        </RowAction>
                      </Flex>
                    </Table.Cell>
                  </Table.Row>
                ))}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Card>
      {actions.dialogs}
    </>
  );
}
