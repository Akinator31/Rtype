// The frame of every screen of the design (node 146:143): the side bar with the logo, the navigation by group
// and the state of the master; the top bar with the path and the title of the page, the state of the live
// connection, the address of the panel and the date; the field of stars behind. Without a session, the page
// to open one; once it expired, the window to reopen it over the page.

import { Box, Flex, Image, Text } from '@chakra-ui/react';
import { useEffect, useRef, useState } from 'react';
import { Outlet } from 'react-router';

import stars from './assets/backgrounds/stars.svg';
import liveSparkle from './assets/flares/live.svg';
import logo from './assets/images/logo.png';
import { AlertStrip } from './components/AlertStrip';
import { Badge, Button, NavItem } from './components/Controls';
import { ToneIcon } from './components/EventFeed';
import { PageTitleContext } from './components/pageTitle';
import { Notifications } from './components/Notifications';
import { notify } from './components/notify';
import { Login, SessionExpired } from './components/Session';
import { toneIcons } from './components/toneIcons';
import { duration, time } from './format';
import { useLive, useLiveStore } from './live/context';
import type { LiveState } from './live/state';
import { usePreferences } from './preferences';
import { strings } from './strings';
import { alertFlareGlow, alertFlareLine, alertGlow, flareGlow, flareLine, glow } from './theme/system';
import { useNow } from './useNow';

const groups = [
  {
    label: strings.nav.groups.server,
    links: [
      { to: '/', label: strings.nav.overview, end: true },
      { to: '/games', label: strings.nav.games },
      { to: '/players', label: strings.nav.players },
      { to: '/bans', label: strings.nav.bans },
    ],
  },
  {
    label: strings.nav.groups.communication,
    links: [
      { to: '/messages', label: strings.nav.messages },
      { to: '/history', label: strings.nav.history },
    ],
  },
  {
    label: strings.nav.groups.administration,
    links: [{ to: '/settings', label: strings.nav.settings }],
  },
];

function Navigation() {
  return (
    <Flex
      as="nav"
      aria-label={strings.nav.label}
      direction={{ base: 'row', lg: 'column' }}
      wrap="wrap"
      gap="4px"
    >
      {groups.map((group) => (
        <Flex key={group.label} direction={{ base: 'row', lg: 'column' }} wrap="wrap" gap="4px">
          <Text
            textStyle="label"
            color="fg.group"
            pl="16px"
            pt="18px"
            pb="6px"
            display={{ base: 'none', lg: 'block' }}
          >
            {group.label}
          </Text>
          {group.links.map((link) => (
            <NavItem key={link.to} to={link.to} end={link.end} w={{ base: 'auto', lg: '208px' }}>
              {link.label}
            </NavItem>
          ))}
        </Flex>
      ))}
    </Flex>
  );
}

/** The state of the master at the foot of the side bar: online, its version and uptime, the session. */
function MasterState({ live, now }: { live: LiveState; now: number }) {
  const lost = live.connection === 'reconnecting';
  const t = strings.master;
  return (
    <Flex
      direction="column"
      gap="10px"
      p="16px"
      borderWidth="1px"
      borderColor={lost ? 'rgba(232,114,110,0.45)' : 'border'}
      borderRadius="l2"
      data-testid="master-state"
    >
      <Flex align="center" justify="space-between">
        <Text textStyle="link" color="fg" textShadow={glow(0.5)}>
          {t.title}
        </Text>
        {lost ? (
          <Badge tone="alert">{t.offline}</Badge>
        ) : live.connection === 'open' ? (
          <Badge tone="active">{t.online}</Badge>
        ) : (
          <Badge tone="muted">{t.waiting}</Badge>
        )}
      </Flex>
      <Text textStyle="hint" color={lost ? 'alert' : 'fg.muted'} truncate>
        {lost
          ? t.silent(live.lastFrameAt === null ? strings.none : time(live.lastFrameAt))
          : live.server !== null
            ? t.running(live.server.version, duration(now - live.server.startedAt))
            : strings.none}
      </Text>
    </Flex>
  );
}

/** The live connection, at the right of the top bar. */
function ConnectionState({ live, now }: { live: LiveState; now: number }) {
  const t = strings.connection;
  const lost = live.connection === 'reconnecting' || live.connection === 'signed-out';
  const label = {
    connecting: t.connecting,
    open: t.open,
    reconnecting: t.reconnecting,
    'signed-out': t.signedOut,
  }[live.connection];
  // Only what the host may act on: when the next attempt comes
  const detail =
    live.connection === 'reconnecting'
      ? t.reconnectingDetail(Math.max(0, Math.ceil(((live.retryAt ?? now) - now) / 1000)))
      : undefined;
  return (
    <Flex align="center" gap="10px" minW={0}>
      {lost ? (
        <ToneIcon tone="alert" />
      ) : (
        <Box position="relative" w="24px" h="8px" flexShrink={0}>
          <Image
            src={liveSparkle}
            alt=""
            position="absolute"
            top="50%"
            left="50%"
            w="39.57px"
            h="24px"
            maxW="none"
            transform="translate(-50%, -50%)"
            opacity={live.connection === 'open' ? 1 : 0.4}
          />
        </Box>
      )}
      <Text
        textStyle="link"
        data-testid="connection"
        data-state={live.connection}
        color={lost ? 'alert' : live.connection === 'open' ? 'fg' : 'fg.muted'}
        textShadow={lost ? alertGlow(0.6) : live.connection === 'open' ? glow(0.6) : 'none'}
        whiteSpace="nowrap"
      >
        {label}
      </Text>
      {detail !== undefined && (
        <Text textStyle="hint" color="fg.muted" whiteSpace="nowrap">
          {detail}
        </Text>
      )}
    </Flex>
  );
}

function TopBar({ title, live, now }: { title: string; live: LiveState; now: number }) {
  const lost = live.connection === 'reconnecting';
  return (
    <Box as="header" position="relative" borderBottomWidth="1px" borderColor="border">
      <Flex
        minH="72px"
        px={{ base: '16px', md: '40px' }}
        py="16px"
        align="center"
        justify="space-between"
        gap="20px"
        wrap="wrap"
      >
        <Text as="h1" textStyle="pageTitle" truncate minW={0}>
          {title}
        </Text>
        <ConnectionState live={live} now={now} />
      </Flex>
      <Box
        position="absolute"
        bottom="-1px"
        left={{ base: '16px', md: '40px' }}
        w="min(480px, 60%)"
        h="1px"
        bgImage={lost ? alertFlareLine : flareLine}
        boxShadow={lost ? alertFlareGlow : flareGlow}
      />
    </Box>
  );
}

/** A short signal when an instance becomes stuck, if the host asked for it. */
function beep(): void {
  try {
    const context = new AudioContext();
    const oscillator = context.createOscillator();
    const gain = context.createGain();
    oscillator.frequency.value = 880;
    gain.gain.setValueAtTime(0.08, context.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.0001, context.currentTime + 0.4);
    oscillator.connect(gain).connect(context.destination);
    oscillator.start();
    oscillator.stop(context.currentTime + 0.4);
    oscillator.onended = () => void context.close();
  } catch {
    // No sound in this browser
  }
}

/** What the shell tells by itself: a reconnection, an instance stuck. */
function useSignals(live: LiveState): void {
  const { soundAlert } = usePreferences();
  const connection = useRef(live.connection);
  const stuck = useRef(new Set<number>());

  useEffect(() => {
    if (connection.current === 'reconnecting' && live.connection === 'open') {
      notify('info', strings.notify.reconnected);
    }
    connection.current = live.connection;
  }, [live.connection]);

  useEffect(() => {
    const now = new Set(live.games.filter((game) => game.stale).map((game) => game.id));
    const fresh = [...now].some((id) => !stuck.current.has(id));
    stuck.current = now;
    if (fresh && soundAlert) beep();
  }, [live.games, soundAlert]);
}

export function Shell() {
  const live = useLive();
  const store = useLiveStore();
  const now = useNow();
  const [title, setTitle] = useState('');
  useSignals(live);

  // Never opened: the page to open a session
  if (live.connection === 'signed-out' && live.server === null) {
    return (
      <Box minH="100vh" bg="bg" bgImage={`url(${stars})`} bgSize="cover" bgAttachment="fixed">
        <Login />
      </Box>
    );
  }

  const lost = live.connection === 'reconnecting';
  return (
    <PageTitleContext.Provider value={setTitle}>
      <Flex
        minH="100vh"
        bg="bg"
        bgImage={`url(${stars})`}
        bgSize="cover"
        backgroundPosition="top right"
        bgAttachment="fixed"
        direction={{ base: 'column', lg: 'row' }}
      >
        <Flex
          as="aside"
          direction="column"
          gap="4px"
          w={{ base: '100%', lg: '240px' }}
          flexShrink={0}
          bg="rgba(0,0,0,0.9)"
          borderRightWidth={{ base: 0, lg: '1px' }}
          borderBottomWidth={{ base: '1px', lg: 0 }}
          borderColor="border"
          px="16px"
          pt={{ base: '16px', lg: '28px' }}
          pb={{ base: '12px', lg: '24px' }}
          position={{ base: 'static', lg: 'sticky' }}
          top="0"
          h={{ base: 'auto', lg: '100vh' }}
          overflowY="auto"
        >
          <Box pl="8px" pb="6px">
            <Image
              src={logo}
              alt={strings.brand}
              w={{ base: '107px', lg: '160px' }}
              h={{ base: '60px', lg: '90px' }}
              objectFit="cover"
            />
          </Box>
          <Navigation />
          <Box flex="1 0 0" minH="24px" display={{ base: 'none', lg: 'block' }} />
          <Box display={{ base: 'none', lg: 'block' }}>
            <MasterState live={live} now={now} />
          </Box>
        </Flex>
        <Flex direction="column" flex="1 1 0" minW={0}>
          <TopBar title={title} live={live} now={now} />
          <Flex as="main" direction="column" gap="24px" px={{ base: '16px', md: '40px' }} py="32px" minW={0}>
            {lost && (
              <AlertStrip
                title={strings.connection.lostTitle}
                detail={strings.connection.lostDetail}
                action={
                  <Button variant="danger" onClick={() => store.retryNow()}>
                    {strings.connection.retry}
                  </Button>
                }
              />
            )}
            <Outlet />
          </Flex>
        </Flex>
      </Flex>
      {/* The icons asked at once: a master lost before one showed could not serve it any more */}
      <Box display="none" aria-hidden>
        {Object.values(toneIcons).map((icon) => (
          <img key={icon} src={icon} alt="" />
        ))}
      </Box>
      {live.connection === 'signed-out' && <SessionExpired />}
      <Notifications />
    </PageTitleContext.Provider>
  );
}
