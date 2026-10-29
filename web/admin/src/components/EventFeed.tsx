// The feed of events of the design (the card "Direct" of the overview, node 146:143): one line per event, its
// subject and object lit, its icon by tone, fading with its rank.

import { Box, Flex, Image, Text } from '@chakra-ui/react';
import type { ReactNode } from 'react';

import { fade } from '../format';
import type { Tone } from '../live/state';
import { alertGlow, glow } from '../theme/system';
import { toneIcons } from './toneIcons';

/** The icon of a tone, 11 px, its glow drawn in the room its SVG keeps around it. */
export function ToneIcon({ tone }: { tone: Tone }) {
  return (
    <Box position="relative" w="11px" h="11px" flexShrink={0}>
      <Image
        src={toneIcons[tone]}
        alt=""
        position="absolute"
        top="50%"
        left="50%"
        maxW="none"
        w={tone === 'info' ? '21.63px' : '22.31px'}
        h={tone === 'bonus' ? '22.31px' : '21.63px'}
        transform="translate(-50%, -50%)"
      />
    </Box>
  );
}

export interface EventLineProps {
  tone: Tone;
  subject: string;
  verb: string;
  object?: string;
  /** Before the line, such as its time. */
  lead?: ReactNode;
  opacity?: number;
}

export function EventLine({ tone, subject, verb, object, lead, opacity = 1 }: EventLineProps) {
  const alert = tone === 'alert';
  return (
    <Flex gap="8px" align="center" opacity={opacity} minW={0} data-tone={tone}>
      {lead}
      <Text
        fontFamily="heading"
        fontSize="9px"
        letterSpacing="1px"
        textTransform="uppercase"
        color={alert ? 'alert' : 'fg'}
        textShadow={alert ? alertGlow(0.5) : glow(0.5)}
        whiteSpace="nowrap"
      >
        {subject}
      </Text>
      <Text fontSize="10px" color="fg.muted" truncate minW={0}>
        {verb}
      </Text>
      {object !== undefined && object !== '' && (
        <Text
          fontFamily="heading"
          fontSize="9px"
          letterSpacing="1px"
          textTransform="uppercase"
          whiteSpace="nowrap"
        >
          {object}
        </Text>
      )}
      <ToneIcon tone={tone} />
    </Flex>
  );
}

export interface FeedItem extends EventLineProps {
  key: number | string;
}

/** Lines stacked, the newest on top, fading with their rank. */
export function EventFeed({ items, empty, label }: { items: FeedItem[]; empty: string; label?: string }) {
  if (items.length === 0) {
    return (
      <Text color="fg.subtle" fontSize="10px">
        {empty}
      </Text>
    );
  }
  return (
    <Flex direction="column" gap="14px" role="log" aria-live="polite" aria-label={label} minW={0}>
      {items.map(({ key, ...line }, rank) => (
        <EventLine key={key} {...line} opacity={fade(rank)} />
      ))}
    </Flex>
  );
}
