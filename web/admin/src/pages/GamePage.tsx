// One game, live: its players, their scores and latency, its wave; end it, or message its players.

import { Flex, Grid, Link as ChakraLink, Table, Text } from '@chakra-ui/react';
import { useState } from 'react';
import { Link, useParams } from 'react-router';

import { EndGameDialog, KickDialog, MessageDialog } from '../components/ActionDialogs';
import { Banner } from '../components/Banner';
import { FlareButton } from '../components/Flare';
import { Gauge } from '../components/Gauge';
import { Empty, Field, PageHeader, Surface } from '../components/Layout';
import { dateTime, duration, score } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { useNow } from '../useNow';
import { GameStatus } from './Games';

type Dialog = { kind: 'end' } | { kind: 'message' } | { kind: 'kick'; player: { id: number; name: string } };

export function GamePage() {
  const { id } = useParams();
  const { games } = useLive();
  const now = useNow();
  const [dialog, setDialog] = useState<Dialog | null>(null);
  const t = strings.game;
  const game = games.find((candidate) => String(candidate.id) === id);

  const back = (
    <ChakraLink asChild textStyle="caption" color="fg.muted" _hover={{ color: 'fg' }}>
      <Link to="/games">← {t.back}</Link>
    </ChakraLink>
  );

  if (game === undefined) {
    return (
      <>
        <PageHeader caption={t.caption} title={id === undefined ? '—' : `#${id}`} />
        <Surface>
          <Empty>{t.notFound}</Empty>
          <Flex justify="center">{back}</Flex>
        </Surface>
      </>
    );
  }

  return (
    <>
      <Flex mb="16px">{back}</Flex>
      <PageHeader
        caption={`${t.caption} ${game.id}`}
        title={game.name}
        actions={
          <>
            <FlareButton onClick={() => setDialog({ kind: 'message' })}>{t.message}</FlareButton>
            <FlareButton tone="alert" onClick={() => setDialog({ kind: 'end' })}>
              {t.end}
            </FlareButton>
          </>
        }
      />
      {game.stale && (
        <Flex mb="16px">
          <Banner tone="alert" title={strings.games.stale} detail={t.staleWarning} />
        </Flex>
      )}
      <Grid templateColumns={{ base: '1fr', lg: '1fr 2fr' }} gap="16px">
        <Surface>
          <Field label={strings.games.status}>
            <GameStatus game={game} />
          </Field>
          <Field label={t.wave}>{game.wave ?? t.unknown}</Field>
          <Field label={t.tick}>{game.tick ?? t.unknown}</Field>
          <Field label={t.port}>{game.port}</Field>
          <Field label={t.created}>{dateTime(game.createdAt)}</Field>
          <Field label={strings.games.age}>{duration(now - game.createdAt)}</Field>
          <Field label={t.statusAge}>{duration(game.statusAge)}</Field>
          <Flex direction="column" gap="8px" mt="16px">
            <Text textStyle="caption" color="fg.muted">
              {t.seats} · {strings.of(game.players.length, game.maxPlayers)}
            </Text>
            <Gauge
              value={game.players.length / game.maxPlayers}
              label={t.seats}
              valueText={strings.of(game.players.length, game.maxPlayers)}
            />
          </Flex>
        </Surface>
        <Surface title={t.players}>
          {game.players.length === 0 ? (
            <Empty>{strings.players.empty}</Empty>
          ) : (
            <Table.ScrollArea>
              <Table.Root aria-label={t.players}>
                <Table.Header>
                  <Table.Row>
                    <Table.ColumnHeader>{strings.players.name}</Table.ColumnHeader>
                    <Table.ColumnHeader textAlign="right">{t.score}</Table.ColumnHeader>
                    <Table.ColumnHeader>{t.alive}</Table.ColumnHeader>
                    <Table.ColumnHeader textAlign="right">{t.latency}</Table.ColumnHeader>
                    <Table.ColumnHeader />
                  </Table.Row>
                </Table.Header>
                <Table.Body>
                  {game.players.map((player) => (
                    <Table.Row key={player.id}>
                      <Table.Cell>{player.name}</Table.Cell>
                      <Table.Cell textStyle="number" textAlign="right">
                        {player.score === null ? t.unknown : score(player.score)}
                      </Table.Cell>
                      <Table.Cell color={player.alive === false ? 'alert' : undefined}>
                        {player.alive === null ? t.unknown : player.alive ? t.yes : t.no}
                      </Table.Cell>
                      <Table.Cell textStyle="number" textAlign="right">
                        {player.latency === null ? t.unknown : `${player.latency} ms`}
                      </Table.Cell>
                      <Table.Cell textAlign="right">
                        <FlareButton
                          size="sm"
                          tone="alert"
                          onClick={() => setDialog({ kind: 'kick', player })}
                          aria-label={`${strings.players.remove} ${player.name}`}
                        >
                          {strings.players.remove}
                        </FlareButton>
                      </Table.Cell>
                    </Table.Row>
                  ))}
                </Table.Body>
              </Table.Root>
            </Table.ScrollArea>
          )}
        </Surface>
      </Grid>
      {dialog?.kind === 'end' && <EndGameDialog game={game} onClose={() => setDialog(null)} />}
      {dialog?.kind === 'message' && <MessageDialog game={game} onClose={() => setDialog(null)} />}
      {dialog?.kind === 'kick' && <KickDialog player={dialog.player} onClose={() => setDialog(null)} />}
    </>
  );
}
