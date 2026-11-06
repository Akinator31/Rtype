// The settings (node 150:822): what the server says of itself, read only; and the preferences of the panel,
// kept in this browser.

import { Box, Flex, Grid, Text } from '@chakra-ui/react';

import { Card, Pair } from '../components/Layout';
import { usePageTitle } from '../components/pageTitle';
import { dateTime } from '../format';
import { useLive } from '../live/context';
import { setPreferences, usePreferences } from '../preferences';
import { strings } from '../strings';
import { glow } from '../theme/system';

/** A switch of the design: Off, a line and its light, On; the side chosen lit. */
function Toggle({
  label,
  checked,
  onChange,
}: {
  label: string;
  checked: boolean;
  onChange: (on: boolean) => void;
}) {
  const t = strings.settings;
  return (
    <Flex
      as="button"
      role="switch"
      aria-checked={checked}
      aria-label={label}
      onClick={() => onChange(!checked)}
      align="center"
      gap="6px"
      bg="transparent"
      cursor="pointer"
      flexShrink={0}
      textStyle="link"
      fontSize="8px"
      _focusVisible={{ outline: 'none', textShadow: glow(0.8) }}
    >
      <Text as="span" color={checked ? 'fg.group' : 'fg'} textShadow={checked ? 'none' : glow(0.5)}>
        {t.off}
      </Text>
      <Box position="relative" w="28px" h="1px" bg="edge">
        <Box
          position="absolute"
          top="50%"
          left={checked ? '100%' : '0'}
          w="5px"
          h="5px"
          borderRadius="full"
          bg="white"
          boxShadow="0 0 6px rgba(255,255,255,0.9)"
          transform="translate(-50%, -50%)"
          transition="left 120ms"
        />
      </Box>
      <Text as="span" color={checked ? 'fg' : 'fg.group'} textShadow={checked ? glow(0.5) : 'none'}>
        {t.on}
      </Text>
    </Flex>
  );
}

function Preference({
  title,
  detail,
  checked,
  onChange,
}: {
  title: string;
  detail: string;
  checked: boolean;
  onChange: (on: boolean) => void;
}) {
  return (
    <Flex
      align="center"
      justify="space-between"
      gap="16px"
      pb="12px"
      borderBottomWidth="1px"
      borderColor="border"
    >
      <Flex direction="column" gap="6px" minW={0}>
        <Text textStyle="link" fontSize="10px" color="fg" textShadow={glow(0.4)}>
          {title}
        </Text>
        <Text textStyle="hint" color="fg.subtle">
          {detail}
        </Text>
      </Flex>
      <Toggle label={title} checked={checked} onChange={onChange} />
    </Flex>
  );
}

export function Settings() {
  const { server } = useLive();
  const preferences = usePreferences();
  const t = strings.settings;
  usePageTitle(t.title);
  return (
    <Grid templateColumns={{ base: '1fr', xl: '1fr 1fr' }} gap="20px" alignItems="start">
      <Card title={t.server}>
        <Pair label={t.version}>{server?.version ?? strings.none}</Pair>
        <Pair label={t.started} plain>
          {server ? dateTime(server.startedAt) : strings.none}
        </Pair>
        <Pair label={t.lobby}>{server ? strings.overview.lobbyValue(server.ports.lobby) : strings.none}</Pair>
        <Pair label={t.games}>
          {server ? strings.overview.gamePortValue(server.ports.game, server.maxGames) : strings.none}
        </Pair>
      </Card>
      <Card title={t.preferences} gap="16px">
        <Preference
          title={t.confirm}
          detail={t.confirmDetail}
          checked={preferences.confirmActions}
          onChange={(on) => setPreferences({ confirmActions: on })}
        />
        <Preference
          title={t.sound}
          detail={t.soundDetail}
          checked={preferences.soundAlert}
          onChange={(on) => setPreferences({ soundAlert: on })}
        />
      </Card>
    </Grid>
  );
}
