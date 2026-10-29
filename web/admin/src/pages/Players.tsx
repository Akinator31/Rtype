// The players of the lobby, in a game or not (node 148:197): searched and filtered; a line opens the card of
// its player, its score and latency, another pilot of its address; remove it, or ban its address.

import { Box, Flex, Grid, Table, Text } from '@chakra-ui/react';
import { useMemo, useState } from 'react';
import { Link } from 'react-router';

import type { Player } from '../api/types';
import { useActions } from '../components/useActions';
import { Badge, Button, Chip, RowAction } from '../components/Controls';
import { ToneIcon } from '../components/EventFeed';
import { Card, Empty, Pair, SearchField, Toolbar } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { duration, score } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { alertGlow, chromeGradient } from '../theme/system';
import { useNow } from '../useNow';

type Filter = 'all' | 'inGame' | 'lobby';

const filters: Record<Filter, (player: Player) => boolean> = {
  all: () => true,
  inGame: (player) => player.gameId !== null,
  lobby: (player) => player.gameId === null,
};

function matches(player: Player, query: string): boolean {
  const needle = query.trim().toLowerCase().replace(/^#/, '');
  return (
    needle === '' ||
    player.name.toLowerCase().includes(needle) ||
    player.address.includes(needle) ||
    String(player.id) === needle
  );
}

function PilotCard({ player, actions }: { player: Player; actions: ReturnType<typeof useActions> }) {
  const { players, games } = useLive();
  const now = useNow();
  const t = strings.players;
  const game = games.find((candidate) => candidate.id === player.gameId);
  const pilot = game?.players.find((candidate) => candidate.id === player.id);
  const others = players.filter((other) => other.address === player.address && other.id !== player.id);
  return (
    <Card gap="16px" data-testid="pilot-card" aria-label={t.card}>
      <Text
        as="h3"
        fontFamily="heading"
        fontSize="24px"
        letterSpacing="2px"
        textTransform="uppercase"
        bgImage={chromeGradient}
        bgClip="text"
        color="transparent"
        filter="drop-shadow(0 0 12px rgba(255,255,255,0.4))"
        truncate
      >
        {player.name}{' '}
        <Text as="span" textStyle="number" color="fg.subtle" fontSize="12px">
          #{player.id}
        </Text>
      </Text>
      <Pair label={t.cardAddress}>{player.address}</Pair>
      <Pair label={t.cardGame}>
        {player.gameId === null ? t.lobby : `${game?.name ?? ''} #${player.gameId}`}
      </Pair>
      <Pair label={t.cardConnected} plain>
        {duration(now - player.joinedAt)}
      </Pair>
      <Pair label={t.cardScore}>{pilot?.score == null ? strings.none : score(pilot.score)}</Pair>
      <Pair label={t.cardLatency} plain>
        {pilot?.latency == null ? strings.none : `${pilot.latency} ms`}
      </Pair>
      {others.length > 0 && (
        <Flex
          direction="column"
          gap="8px"
          px="20px"
          py="16px"
          borderWidth="1px"
          borderColor="rgba(232,114,110,0.45)"
          borderRadius="l2"
          bg="rgba(232,114,110,0.04)"
          role="note"
        >
          <Flex align="center" gap="10px">
            <ToneIcon tone="alert" />
            <Text
              fontFamily="heading"
              fontSize="10px"
              letterSpacing="2px"
              textTransform="uppercase"
              color="alert"
              textShadow={alertGlow(0.5)}
            >
              {t.shared}
            </Text>
          </Flex>
          <Text textStyle="body" color="fg.muted">
            {t.sharedDetail(others.map((other) => `${other.name.toUpperCase()} #${other.id}`).join(', '))}
          </Text>
        </Flex>
      )}
      <Button w="100%" justifyContent="flex-start" onClick={() => actions.kick(player)}>
        {t.removeFromServer}
      </Button>
      <Button
        w="100%"
        justifyContent="flex-start"
        variant="danger"
        onClick={() => actions.ban(player.address)}
      >
        {t.banAddress}
      </Button>
    </Card>
  );
}

export function Players() {
  const { players, games } = useLive();
  const actions = useActions();
  const [query, setQuery] = useState('');
  const [filter, setFilter] = useState<Filter>('all');
  const [chosen, setChosen] = useState<number | null>(null);
  const now = useNow();
  const t = strings.players;
  usePageTitle(t.title);

  const shown = useMemo(
    () => players.filter((player) => filters[filter](player) && matches(player, query)),
    [players, query, filter],
  );
  const selected = players.find((player) => player.id === chosen);
  // The card of a player opens and closes on its line
  const choose = (id: number) => setChosen((current) => (current === id ? null : id));

  return (
    <>
      <Toolbar>
        <SearchField label={t.search} value={query} onChange={setQuery} />
        <Flex gap="12px" wrap="wrap" role="group" aria-label={t.game}>
          {(['all', 'inGame', 'lobby'] as const).map((which) => (
            <Chip key={which} pressed={filter === which} onClick={() => setFilter(which)}>
              {t[which]} · {players.filter(filters[which]).length}
            </Chip>
          ))}
        </Flex>
      </Toolbar>
      <Grid
        templateColumns={{ base: '1fr', xl: selected === undefined ? '1fr' : '1fr 340px' }}
        gap="20px"
        alignItems="start"
      >
        <Card>
          {shown.length === 0 ? (
            <Empty>{players.length === 0 ? t.empty : t.noMatch}</Empty>
          ) : (
            <Table.ScrollArea>
              <Table.Root aria-label={t.table}>
                <Table.Header>
                  <Table.Row>
                    <Table.ColumnHeader>{t.pilot}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.address}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.game}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.connected}</Table.ColumnHeader>
                    <Table.ColumnHeader />
                  </Table.Row>
                </Table.Header>
                <Table.Body>
                  {shown.map((player) => {
                    const game = games.find((candidate) => candidate.id === player.gameId);
                    return (
                      <Table.Row
                        key={player.id}
                        data-testid={`player-${player.id}`}
                        aria-selected={player.id === chosen}
                        cursor="pointer"
                        onClick={() => choose(player.id)}
                      >
                        <Table.Cell>
                          <Box
                            as="button"
                            textStyle="name"
                            bg="transparent"
                            cursor="pointer"
                            aria-expanded={player.id === chosen}
                            onClick={(event) => {
                              event.stopPropagation();
                              choose(player.id);
                            }}
                          >
                            {player.name}
                          </Box>
                        </Table.Cell>
                        <Table.Cell>{player.address}</Table.Cell>
                        <Table.Cell>
                          {player.gameId === null ? (
                            <Badge tone="neutral">{t.lobby}</Badge>
                          ) : (
                            <Text asChild textStyle="number" color="fg" textDecoration="none">
                              <Link
                                to={`/games/${player.gameId}`}
                                onClick={(event) => event.stopPropagation()}
                              >
                                {game?.name ?? ''} #{player.gameId}
                              </Link>
                            </Text>
                          )}
                        </Table.Cell>
                        <Table.Cell whiteSpace="nowrap">{duration(now - player.joinedAt)}</Table.Cell>
                        <Table.Cell>
                          <Flex justify="flex-end" gap="10px" onClick={(event) => event.stopPropagation()}>
                            <RowAction
                              onClick={() => actions.kick(player)}
                              aria-label={`${t.remove} ${player.name}`}
                            >
                              {t.remove}
                            </RowAction>
                            <RowAction
                              tone="alert"
                              onClick={() => actions.ban(player.address)}
                              aria-label={`${t.ban} ${player.address}`}
                            >
                              {t.ban}
                            </RowAction>
                          </Flex>
                        </Table.Cell>
                      </Table.Row>
                    );
                  })}
                </Table.Body>
              </Table.Root>
            </Table.ScrollArea>
          )}
        </Card>
        {selected !== undefined && <PilotCard player={selected} actions={actions} />}
      </Grid>
      {actions.dialogs}
    </>
  );
}
