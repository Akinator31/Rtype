// One game, live (node 147:432): its wave and its time of play; its pilots with their address,
// score, state and latency; a message to its players; its own events. End it, remove a pilot, mute its voice, ban
// its address.

import { Box, Flex, Grid, Table, Text } from '@chakra-ui/react';
import type { FormEvent } from 'react';
import { useParams } from 'react-router';

import type { Game, GamePlayer, Player } from '../api/types';
import { useActions } from '../components/useActions';
import { AlertStrip } from '../components/AlertStrip';
import { Badge, Button, ButtonLink, RowAction } from '../components/Controls';
import { EventFeed } from '../components/EventFeed';
import { Card, Empty } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { MessageField } from '../components/MessageField';
import { useSendMessage } from '../components/useSendMessage';
import { dayTime, duration, score, tenths, wave } from '../format';
import { useLive } from '../live/context';
import { strings } from '../strings';
import { glow } from '../theme/system';
import { useNow } from '../useNow';
import { gameRef } from '../records';
import { GameState } from './Overview';

/** The latency of a pilot: four bars lit by how good it is, then its milliseconds. */
function Latency({ value }: { value: number | null }) {
  if (value === null) {
    return <>{strings.game.notMeasured}</>;
  }
  const lit = value < 40 ? 4 : value < 80 ? 3 : value < 150 ? 2 : 1;
  return (
    <Flex align="center" gap="8px">
      <Flex align="flex-end" gap="1.5px" h="9px" aria-hidden>
        {[3, 5, 7, 9].map((height, index) => (
          <Box key={height} w="2px" h={`${height}px`} bg={index < lit ? 'fg' : 'edge'} />
        ))}
      </Flex>
      <Text as="span" textStyle="number" color="fg">
        {value} ms
      </Text>
    </Flex>
  );
}

function PilotState({ pilot }: { pilot: GamePlayer }) {
  if (pilot.alive === null) return <Badge tone="neutral">{strings.game.joining}</Badge>;
  return pilot.alive ? (
    <Badge tone="active">{strings.game.alive}</Badge>
  ) : (
    <Badge tone="muted">{strings.game.destroyed}</Badge>
  );
}

/** Its voice muted by an administrator: for this game, or as its account until a date. */
function VoiceState({ pilot }: { pilot: GamePlayer }) {
  const until = pilot.voiceMutedUntil ?? null;
  if (pilot.voiceMuted !== true && until === null) return null;
  return (
    <Badge tone="alert" title={until === null ? undefined : strings.game.voiceMutedUntil(dayTime(until))}>
      {strings.game.voiceMuted}
    </Badge>
  );
}

function Figure({ label, value }: { label: string; value: string }) {
  return (
    <Flex direction="column" gap="6px">
      <Text textStyle="label" color="fg.subtle">
        {label}
      </Text>
      <Text
        fontFamily="heading"
        fontSize="18px"
        letterSpacing="1px"
        textTransform="uppercase"
        textShadow={glow(0.4)}
      >
        {value}
      </Text>
    </Flex>
  );
}

function Pilots({
  game,
  players,
  actions,
}: {
  game: Game;
  players: Player[];
  actions: ReturnType<typeof useActions>;
}) {
  const t = strings.game;
  return (
    <Card title={t.pilots}>
      {game.players.length === 0 ? (
        <Empty>{t.noPilot}</Empty>
      ) : (
        <Table.ScrollArea>
          <Table.Root aria-label={t.pilots}>
            <Table.Header>
              <Table.Row>
                <Table.ColumnHeader>{t.pilot}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.address}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.score}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.state}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.latency}</Table.ColumnHeader>
                <Table.ColumnHeader />
              </Table.Row>
            </Table.Header>
            <Table.Body>
              {game.players.map((pilot) => {
                const player = players.find((candidate) => candidate.id === pilot.id);
                return (
                  <Table.Row key={pilot.id} data-testid={`pilot-${pilot.id}`}>
                    <Table.Cell textStyle="name">{pilot.name}</Table.Cell>
                    <Table.Cell>{player?.address ?? strings.none}</Table.Cell>
                    <Table.Cell textStyle="number" color="fg" textShadow={glow(0.35)}>
                      {pilot.score === null ? (
                        <Text as="span" textStyle="body" color="fg.muted" textShadow="none">
                          {t.notConnected}
                        </Text>
                      ) : (
                        score(pilot.score)
                      )}
                    </Table.Cell>
                    <Table.Cell>
                      <Flex gap="8px" wrap="wrap">
                        <PilotState pilot={pilot} />
                        <VoiceState pilot={pilot} />
                      </Flex>
                    </Table.Cell>
                    <Table.Cell>
                      <Latency value={pilot.latency} />
                    </Table.Cell>
                    <Table.Cell>
                      <Flex justify="flex-end" gap="10px">
                        {game.voice !== false && (
                          <RowAction
                            onClick={() => actions.voice(pilot, pilot.voiceMuted !== true)}
                            aria-label={`${pilot.voiceMuted === true ? t.unmuteVoice : t.muteVoice} ${pilot.name}`}
                          >
                            {pilot.voiceMuted === true ? t.unmuteVoice : t.muteVoice}
                          </RowAction>
                        )}
                        <RowAction
                          onClick={() => actions.kick(pilot)}
                          aria-label={`${t.remove} ${pilot.name}`}
                        >
                          {t.remove}
                        </RowAction>
                        {player !== undefined && (
                          <RowAction
                            tone="alert"
                            onClick={() => actions.ban(player.address)}
                            aria-label={`${t.banAddress} ${player.address}`}
                          >
                            {t.banAddress}
                          </RowAction>
                        )}
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
  );
}

function MessageCard({ game }: { game: Game }) {
  const message = useSendMessage(game);
  const t = strings.game;
  const submit = (event: FormEvent) => {
    event.preventDefault();
    void message.send();
  };
  return (
    <Card title={t.messageTitle} as="form" onSubmit={submit} gap="16px">
      <MessageField value={message.text} onChange={message.setText} />
      <Flex>
        <Button type="submit" variant="primary" disabled={!message.ready}>
          {strings.messages.send(game.players.length)}
        </Button>
      </Flex>
    </Card>
  );
}

export function GamePage() {
  const { id } = useParams();
  const { games, players, feed } = useLive();
  const now = useNow();
  const actions = useActions();
  const t = strings.game;
  const game = games.find((candidate) => String(candidate.id) === id);
  usePageTitle(game === undefined ? `#${id ?? ''}` : `${game.name} · #${game.id}`);

  if (game === undefined) {
    return (
      <Card>
        <Empty>{t.notFound}</Empty>
        <Flex justify="center">
          <ButtonLink to="/games">{t.back}</ButtonLink>
        </Flex>
      </Card>
    );
  }

  const events = feed.filter((entry) => entry.gameId === game.id).slice(0, 8);
  return (
    <>
      {game.stale && (
        <AlertStrip
          title={strings.games.stuckTitle}
          detail={strings.games.stuckDetail(`${game.name} #${game.id}`, tenths(game.statusAge))}
        />
      )}
      <Card>
        <Flex gap="36px" align="center" wrap="wrap">
          <Figure label={t.wave} value={game.wave === null ? strings.none : wave(game.wave)} />
          <Figure label={t.playing} value={duration(now - game.createdAt)} />
          <Box flex="1 0 0" />
          <Flex gap="8px">
            <GameState game={game} />
            <Badge tone="neutral">{t.seats(game.players.length, game.maxPlayers)}</Badge>
            {game.voice === false && <Badge tone="muted">{t.noVoice}</Badge>}
          </Flex>
          <Button variant="danger" onClick={() => actions.end(gameRef(game))}>
            {t.end}
          </Button>
        </Flex>
      </Card>
      <Pilots game={game} players={players} actions={actions} />
      <Grid templateColumns={{ base: '1fr', xl: '1fr 470px' }} gap="20px">
        <MessageCard game={game} />
        <Card title={t.events} gap="14px">
          <EventFeed items={events} empty={t.noEvent} label={t.events} />
        </Card>
      </Grid>
      {actions.dialogs}
    </>
  );
}
