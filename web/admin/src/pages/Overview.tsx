// The server at a glance (node 146:143): its four indicators, each opening its page, the games running, the live
// feed, the players of the last hour and the latest actions. The master silent (node 153:881): its last values
// dimmed, and the last events received.

import { Flex, Grid, Table, Text } from '@chakra-ui/react';
import { useEffect, useState } from 'react';
import { Link } from 'react-router';

import { api } from '../api/client';
import type { Action, Ban, Game, ServerInfo } from '../api/types';
import { AttendanceChart } from '../components/AttendanceChart';
import { Badge } from '../components/Controls';
import { EventFeed } from '../components/EventFeed';
import { Card, Empty, Stat } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { duration, wave } from '../format';
import { useLive } from '../live/context';
import { counts } from '../live/state';
import { actionBadge, merged, summaryLine, targetLabel } from '../records';
import { strings } from '../strings';
import { glow } from '../theme/system';
import { useNow } from '../useNow';

/** How often what the events do not carry, the bans, is asked again. */
const ServerRefresh = 30000;

/** The state of a game, as its badge: running, stuck, or waiting for its first status. */
export function GameState({ game }: { game: Game }) {
  if (game.stale) return <Badge tone="alert">{strings.games.stuck}</Badge>;
  if (game.tick === null) return <Badge tone="neutral">{strings.games.waiting}</Badge>;
  return <Badge tone="active">{strings.games.running}</Badge>;
}

function GamesCard({ games }: { games: Game[] }) {
  const t = strings.games;
  return (
    <Card
      title={strings.overview.runningGames}
      link={{ to: '/games', label: strings.overview.seeAll }}
      gap="6px"
    >
      {games.length === 0 ? (
        <Empty>{t.empty}</Empty>
      ) : (
        <Table.ScrollArea>
          <Table.Root aria-label={strings.overview.runningGames}>
            <Table.Header>
              <Table.Row>
                <Table.ColumnHeader ps="0">{t.name}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.seats}</Table.ColumnHeader>
                <Table.ColumnHeader>{t.wave}</Table.ColumnHeader>
                <Table.ColumnHeader textAlign="right">{t.state}</Table.ColumnHeader>
              </Table.Row>
            </Table.Header>
            <Table.Body>
              {games.slice(0, 5).map((game) => (
                <Table.Row key={game.id}>
                  <Table.Cell ps="0">
                    <Text asChild textStyle="name" textDecoration="none">
                      <Link to={`/games/${game.id}`}>{game.name}</Link>
                    </Text>
                  </Table.Cell>
                  <Table.Cell textStyle="number" color="fg.silver">
                    {t.seatsValue(game.players.length, game.maxPlayers)}
                  </Table.Cell>
                  <Table.Cell textStyle="number" color="fg.silver">
                    {game.wave === null ? strings.none : wave(game.wave)}
                  </Table.Cell>
                  <Table.Cell textAlign="right">
                    <GameState game={game} />
                  </Table.Cell>
                </Table.Row>
              ))}
            </Table.Body>
          </Table.Root>
        </Table.ScrollArea>
      )}
    </Card>
  );
}

function LatestActions({ actions }: { actions: Action[] }) {
  return (
    <Card title={strings.overview.latestActions} link={{ to: '/history', label: strings.overview.history }}>
      {actions.length === 0 ? (
        <Empty>{strings.overview.noAction}</Empty>
      ) : (
        <Flex direction="column" gap="12px" role="list" aria-label={strings.overview.latestActions}>
          {actions.slice(0, 4).map((action) => {
            const badge = actionBadge(action);
            return (
              <Flex
                key={action.id}
                role="listitem"
                align="center"
                gap="12px"
                pb="10px"
                borderBottomWidth="1px"
                borderColor="border"
                minW={0}
                data-action={action.action}
              >
                <Badge tone={badge.tone}>{badge.label}</Badge>
                <Text textStyle="name" textShadow={glow(0.35)} truncate>
                  {targetLabel(action)}
                </Text>
                <Text textStyle="hint" color="fg.subtle" truncate>
                  {summaryLine(action)}
                </Text>
              </Flex>
            );
          })}
        </Flex>
      )}
    </Card>
  );
}

export function Overview() {
  const live = useLive();
  const now = useNow();
  const [server, setServer] = useState<ServerInfo | null>(null);
  const [bans, setBans] = useState<Ban[]>([]);
  const [recorded, setRecorded] = useState<Action[]>([]);
  const latestAction = live.actions[0]?.id;
  const t = strings.overview;
  usePageTitle(t.title);

  // The bans change with the actions, and expire on their own: asked again after each action, and now and then
  useEffect(() => {
    let current = true;
    const load = () => {
      api
        .server()
        .then((answer) => current && setServer(answer))
        .catch(() => undefined);
      api
        .bans()
        .then((answer) => current && setBans(answer))
        .catch(() => undefined);
    };
    load();
    const timer = setInterval(load, ServerRefresh);
    return () => {
      current = false;
      clearInterval(timer);
    };
  }, [latestAction]);

  useEffect(() => {
    let current = true;
    api
      .actions({ limit: 4 })
      .then((answer) => current && setRecorded(answer))
      .catch(() => undefined);
    return () => {
      current = false;
    };
  }, []);

  const info = server ?? live.server;
  const { players, games, playing } = counts(live);
  const stuck = live.games.filter((game) => game.stale).length;
  const nextEnd = bans.reduce<number | null>(
    (soonest, ban) => (soonest === null ? ban.expiresAt : Math.min(soonest, ban.expiresAt)),
    null,
  );
  const lost = live.connection === 'reconnecting';

  return (
    <>
      <Grid templateColumns={{ base: '1fr', sm: '1fr 1fr', xl: 'repeat(4, 1fr)' }} gap="20px">
        <Stat
          label={t.players}
          value={players}
          to="/players"
          frozen={lost}
          detail={lost ? undefined : t.playersDetail(playing, players - playing)}
        />
        <Stat
          label={t.games}
          value={games}
          to="/games"
          frozen={lost}
          tone="alert"
          detail={!lost && stuck > 0 ? t.stuck(stuck) : undefined}
        />
        <Stat
          label={t.bans}
          value={info?.bans ?? strings.none}
          to="/bans"
          frozen={lost}
          detail={lost || nextEnd === null ? undefined : t.nextBanEnd(duration(nextEnd - now))}
        />
        <Stat
          label={t.uptime}
          value={info ? duration((lost ? (live.lastFrameAt ?? now) : now) - info.startedAt) : strings.none}
          frozen={lost}
        />
      </Grid>
      {lost ? (
        <Card title={t.lastEvents} minH="300px">
          <EventFeed items={live.feed.slice(0, 12)} empty={t.liveEmpty} label={t.lastEvents} />
        </Card>
      ) : (
        <>
          <Grid templateColumns={{ base: '1fr', xl: '1fr 360px' }} gap="20px">
            <GamesCard games={live.games} />
            <Card title={t.live} gap="14px">
              <EventFeed items={live.feed.slice(0, 7)} empty={t.liveEmpty} label={t.live} />
            </Card>
          </Grid>
          <Grid templateColumns={{ base: '1fr', xl: '1fr 430px' }} gap="20px">
            <Card title={t.attendance} gap="16px" minH="220px">
              <AttendanceChart samples={live.history} now={now} />
            </Card>
            <LatestActions actions={merged(live.actions, recorded)} />
          </Grid>
        </>
      )}
    </>
  );
}
