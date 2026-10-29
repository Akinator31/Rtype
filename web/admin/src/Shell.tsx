// The frame of every page: the logo, the navigation lit by the flare, the state of the live connection, and
// a banner when it is lost.

import { Box, Flex, Image, Text } from '@chakra-ui/react';
import { Outlet } from 'react-router';

import logo from './assets/images/logo.png';
import { Banner } from './components/Banner';
import { FlareLink } from './components/Flare';
import { Notifications } from './components/Notifications';
import { useLive } from './live/context';
import type { Connection } from './live/state';
import { strings } from './strings';

const connectionLabels: Record<Connection, string> = {
  connecting: strings.connection.connecting,
  open: strings.connection.open,
  reconnecting: strings.connection.reconnecting,
  'signed-out': strings.connection.signedOut,
};

function ConnectionState({ connection }: { connection: Connection }) {
  const live = connection === 'open';
  const lost = connection === 'reconnecting' || connection === 'signed-out';
  return (
    <Text
      textStyle="caption"
      data-testid="connection"
      data-state={connection}
      color={live ? 'fg' : lost ? 'alert' : 'fg.muted'}
      textShadow={live ? '0 0 8px rgba(255,255,255,0.6)' : lost ? '0 0 8px rgba(232,114,110,0.6)' : 'none'}
      whiteSpace="nowrap"
    >
      {connectionLabels[connection]}
    </Text>
  );
}

export function Shell() {
  const { connection } = useLive();
  return (
    <Box minH="100vh" bg="bg">
      <Box as="header" borderBottomWidth="1px" borderColor="border">
        <Flex
          maxW="1200px"
          mx="auto"
          px={{ base: '16px', md: '32px' }}
          py="12px"
          align="center"
          justify="space-between"
          gap="16px"
          wrap="wrap"
        >
          <Flex align="center" gap="16px">
            <Image src={logo} alt={strings.brand} h="40px" w="71px" objectFit="cover" mixBlendMode="screen" />
            <Text textStyle="caption" color="fg.muted">
              {strings.panel}
            </Text>
          </Flex>
          <Flex
            as="nav"
            aria-label="Pages"
            wrap="wrap"
            order={{ base: 3, md: 0 }}
            w={{ base: '100%', md: 'auto' }}
          >
            <FlareLink to="/" end>
              {strings.nav.overview}
            </FlareLink>
            <FlareLink to="/games">{strings.nav.games}</FlareLink>
            <FlareLink to="/players">{strings.nav.players}</FlareLink>
            <FlareLink to="/actions">{strings.nav.actions}</FlareLink>
          </Flex>
          <ConnectionState connection={connection} />
        </Flex>
      </Box>
      {connection === 'reconnecting' && (
        <Banner tone="alert" title={strings.connection.lostTitle} detail={strings.connection.lostDetail} />
      )}
      {connection === 'signed-out' && (
        <Banner
          tone="alert"
          title={strings.connection.signedOutTitle}
          detail={strings.connection.signedOutDetail}
        />
      )}
      <Box as="main" maxW="1200px" mx="auto" px={{ base: '16px', md: '32px' }} py="32px">
        <Outlet />
      </Box>
      <Notifications />
    </Box>
  );
}
