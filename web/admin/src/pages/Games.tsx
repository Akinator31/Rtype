// The running games, live: each line updated by the status its instance sends every second.

import { Flex, Input, Link as ChakraLink, NativeSelect, Table, Text } from '@chakra-ui/react';
import { useMemo, useState } from 'react';
import { Link } from 'react-router';

import type { Game } from '../api/types';
import { Gauge } from '../components/Gauge';
import { Empty, PageHeader, Surface } from '../components/Layout';
import { duration } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { useNow } from '../useNow';

type Filter = 'all' | 'stale' | 'room';

const filters: Record<Filter, (game: Game) => boolean> = {
  all: () => true,
  stale: (game) => game.stale,
  room: (game) => game.players.length < game.maxPlayers,
};

/** The state of a game, as a word: starting before its first status, stale when its instance went quiet. */
export function GameStatus({ game }: { game: Game }) {
  const t = strings.games;
  if (game.stale) {
    return (
      <Text as="span" textStyle="caption" color="alert">
        {t.stale}
      </Text>
    );
  }
  return (
    <Text as="span" textStyle="caption" color={game.tick === null ? 'fg.muted' : 'fg'}>
      {game.tick === null ? t.waiting : t.live}
    </Text>
  );
}

export function Games() {
  const { games } = useLive();
  const now = useNow();
  const [query, setQuery] = useState('');
  const [filter, setFilter] = useState<Filter>('all');
  const t = strings.games;

  const shown = useMemo(() => {
    const needle = query.trim().toLowerCase();
    return games.filter((game) => filters[filter](game) && game.name.toLowerCase().includes(needle));
  }, [games, query, filter]);

  return (
    <>
      <PageHeader caption={t.caption} title={t.title} description={t.description} />
      <Surface>
        <Flex gap="24px" mb="16px" wrap="wrap">
          <Input
            placeholder={t.filter}
            aria-label={t.filter}
            value={query}
            onChange={(event) => setQuery(event.target.value)}
            maxW="280px"
          />
          <NativeSelect.Root maxW="200px">
            <NativeSelect.Field
              aria-label={t.status}
              value={filter}
              onChange={(event) => setFilter(event.target.value as Filter)}
            >
              <option value="all">{t.all}</option>
              <option value="stale">{t.onlyStale}</option>
              <option value="room">{t.withRoom}</option>
            </NativeSelect.Field>
            <NativeSelect.Indicator />
          </NativeSelect.Root>
        </Flex>
        {shown.length === 0 ? (
          <Empty>{games.length === 0 ? t.empty : t.noMatch}</Empty>
        ) : (
          <Table.ScrollArea>
            <Table.Root aria-label={t.title}>
              <Table.Header>
                <Table.Row>
                  <Table.ColumnHeader>{t.id}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.name}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.players}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.wave}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.age}</Table.ColumnHeader>
                  <Table.ColumnHeader>{t.status}</Table.ColumnHeader>
                </Table.Row>
              </Table.Header>
              <Table.Body>
                {shown.map((game) => (
                  <Table.Row key={game.id} data-testid={`game-${game.id}`}>
                    <Table.Cell textStyle="number">{game.id}</Table.Cell>
                    <Table.Cell>
                      <ChakraLink
                        asChild
                        color="fg"
                        textDecoration="none"
                        _hover={{ textShadow: '0 0 8px rgba(255,255,255,0.6)' }}
                      >
                        <Link to={`/games/${game.id}`}>{game.name}</Link>
                      </ChakraLink>
                    </Table.Cell>
                    <Table.Cell>
                      <Flex align="center" gap="12px">
                        <Gauge
                          value={game.players.length / game.maxPlayers}
                          label={t.players}
                          valueText={strings.of(game.players.length, game.maxPlayers)}
                          width="80px"
                        />
                        <Text as="span" textStyle="number">
                          {game.players.length}/{game.maxPlayers}
                        </Text>
                      </Flex>
                    </Table.Cell>
                    <Table.Cell textStyle="number">{game.wave ?? '—'}</Table.Cell>
                    <Table.Cell>{duration(now - game.createdAt)}</Table.Cell>
                    <Table.Cell>
                      <GameStatus game={game} />
                    </Table.Cell>
                  </Table.Row>
                ))}
              </Table.Body>
            </Table.Root>
          </Table.ScrollArea>
        )}
      </Surface>
    </>
  );
}
