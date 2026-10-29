// The feed of events of the design (N6, node 44:37): one line per event, its subject and object lit, its
// icon by tone, fading from 100 % to 22 % with age.

import { Box, Flex, Image, Text } from '@chakra-ui/react';
import type { ReactNode } from 'react';

import alertIcon from '../assets/icons/alert.svg';
import bonusIcon from '../assets/icons/bonus.svg';
import infoIcon from '../assets/icons/info.svg';
import { fade } from '../format';
import type { Tone } from '../live/state';

const toneIcons: Record<Tone, string> = { info: infoIcon, bonus: bonusIcon, alert: alertIcon };

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
  const lit = tone === 'alert' ? 'alert' : 'fg';
  const glow = tone === 'alert' ? '0 0 8px rgba(232,114,110,0.5)' : '0 0 8px rgba(255,255,255,0.5)';
  return (
    <Flex gap="8px" align="center" opacity={opacity} minW={0} data-tone={tone}>
      {lead}
      <Text
        fontFamily="heading"
        fontSize="9px"
        letterSpacing="1px"
        textTransform="uppercase"
        color={lit}
        textShadow={glow}
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

/** Lines stacked to the right, the newest on top, fading with their rank. */
export function EventFeed({ items, empty }: { items: FeedItem[]; empty: string }) {
  if (items.length === 0) {
    return (
      <Text color="fg.subtle" fontSize="10px">
        {empty}
      </Text>
    );
  }
  return (
    <Flex direction="column" align="flex-end" gap="10px" role="log" aria-live="polite">
      {items.map(({ key, ...line }, rank) => (
        <EventLine key={key} {...line} opacity={fade(rank)} />
      ))}
    </Flex>
  );
}
