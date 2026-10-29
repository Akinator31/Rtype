// The server at a glance: its counts, its uptime and ports, its games over time, and the latest events.

import { Grid } from '@chakra-ui/react';
import { useEffect, useState } from 'react';

import { api } from '../api/client';
import type { ServerInfo } from '../api/types';
import { MessageDialog } from '../components/ActionDialogs';
import { EventFeed } from '../components/EventFeed';
import { FlareButton } from '../components/Flare';
import { Gauge } from '../components/Gauge';
import { HistoryChart } from '../components/HistoryChart';
import { Field, PageHeader, Stat, Surface } from '../components/Layout';
import { duration } from '../format';
import { useLive } from '../live/context';
import { counts } from '../live/state';
import { strings } from '../strings';
import { useNow } from '../useNow';

/** How often the counts the events do not carry, the bans, are asked again. */
const ServerRefresh = 30000;

export function Overview() {
  const live = useLive();
  const now = useNow();
  const [server, setServer] = useState<ServerInfo | null>(null);
  const [messaging, setMessaging] = useState(false);
  const latestAction = live.actions[0]?.id;

  // The bans change with the actions, and expire on their own: asked again after each action, and now and then
  useEffect(() => {
    let current = true;
    const load = () =>
      api
        .server()
        .then((answer) => current && setServer(answer))
        .catch(() => undefined);
    void load();
    const timer = setInterval(load, ServerRefresh);
    return () => {
      current = false;
      clearInterval(timer);
    };
  }, [latestAction]);

  const info = server ?? live.server;
  const { players, games, playing } = counts(live);
  const t = strings.overview;
  return (
    <>
      <PageHeader
        caption={t.caption}
        title={t.title}
        description={t.description}
        actions={<FlareButton onClick={() => setMessaging(true)}>{t.messageEveryone}</FlareButton>}
      />
      <Grid templateColumns={{ base: '1fr 1fr', lg: 'repeat(4, 1fr)' }} gap="16px" mb="16px">
        <Stat label={t.players} value={players} />
        <Stat label={t.playing} value={playing}>
          <Gauge
            value={players === 0 ? 0 : playing / players}
            label={t.playing}
            valueText={strings.of(playing, players)}
          />
        </Stat>
        <Stat label={t.games} value={games} />
        <Stat label={t.bans} value={info?.bans ?? '—'} />
      </Grid>
      <Grid templateColumns={{ base: '1fr', lg: '1fr 2fr' }} gap="16px" mb="16px">
        <Surface title={t.caption}>
          <Field label={t.version}>{info?.version ?? '—'}</Field>
          <Field label={t.uptime}>{info ? duration(now - info.startedAt) : '—'}</Field>
          <Field label={t.lobbyPort}>{info?.ports.lobby ?? '—'}</Field>
          <Field label={t.gamePorts}>
            {info ? `${info.ports.games.first}–${info.ports.games.last}` : '—'}
          </Field>
          <Field label={t.panelPort}>{info?.ports.panel ?? '—'}</Field>
        </Surface>
        <Surface title={t.history}>
          <HistoryChart samples={live.history} empty={t.historyEmpty} label={t.history} />
        </Surface>
      </Grid>
      <Surface title={t.feed}>
        <EventFeed items={live.feed} empty={t.feedEmpty} />
      </Surface>
      {messaging && <MessageDialog game={null} onClose={() => setMessaging(false)} />}
    </>
  );
}
