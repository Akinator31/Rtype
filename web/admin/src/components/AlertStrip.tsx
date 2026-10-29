// The strip of the design's screens for what stops the host (node 147:248): an instance stuck, the master
// unreachable, a step to come. Red for an alert, silver for an information; its action on the right.

import { Box, Flex, Text } from '@chakra-ui/react';
import type { ReactNode } from 'react';

import { alertGlow, glow } from '../theme/system';
import { ToneIcon } from './EventFeed';

export function AlertStrip({
  title,
  detail,
  tone = 'alert',
  action,
}: {
  title: string;
  detail?: ReactNode;
  tone?: 'info' | 'alert';
  action?: ReactNode;
}) {
  const alert = tone === 'alert';
  return (
    <Flex
      role={alert ? 'alert' : 'status'}
      data-tone={tone}
      align="center"
      gap="16px"
      wrap={{ base: 'wrap', md: 'nowrap' }}
      pl="20px"
      pr="16px"
      py="14px"
      minH="67px"
      borderWidth="1px"
      borderRadius="l2"
      borderColor={alert ? 'rgba(232,114,110,0.45)' : 'edge'}
      bgImage={
        alert
          ? 'linear-gradient(90deg, rgba(232,114,110,0.1) 0%, rgba(232,114,110,0) 100%)'
          : 'linear-gradient(90deg, rgba(255,255,255,0.04) 0%, rgba(255,255,255,0) 100%)'
      }
    >
      <ToneIcon tone={alert ? 'alert' : 'info'} />
      <Text
        fontFamily="heading"
        fontSize="10px"
        letterSpacing="2px"
        textTransform="uppercase"
        whiteSpace="nowrap"
        color={alert ? 'alert' : 'fg'}
        textShadow={alert ? alertGlow(0.5) : glow(0.5)}
      >
        {title}
      </Text>
      {detail !== undefined && (
        <Text textStyle="body" color="fg.muted" minW={0}>
          {detail}
        </Text>
      )}
      <Box flex="1 0 0" />
      {action}
    </Flex>
  );
}
