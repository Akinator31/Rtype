// The round picture of an account (the avatar of the game, Série 14): its Discord picture, served by the master
// at /api/accounts/{id}/avatar and named by its version so that the browser keeps it; else its initial. The ring
// is lit for an account linked to Discord.

import { Box, Flex, Image } from '@chakra-ui/react';

import type { DiscordProfile } from '../api/types';
import { initialOf } from '../format';
import { glow } from '../theme/system';

export function Avatar({
  accountId,
  name,
  discord,
  size = 28,
}: {
  accountId: number;
  name: string;
  discord: DiscordProfile | null | undefined;
  size?: number;
}) {
  const linked = discord !== null && discord !== undefined;
  const picture = linked && discord.avatar !== null;
  const inner = size - Math.max(4, Math.round(size / 10));
  return (
    <Flex
      align="center"
      justify="center"
      flexShrink={0}
      w={`${size}px`}
      h={`${size}px`}
      borderRadius="full"
      borderWidth="1px"
      borderColor={linked ? 'rgba(255,255,255,0.9)' : 'rgba(255,255,255,0.45)'}
      boxShadow={linked ? glow(0.45) : undefined}
      data-testid={`avatar-${accountId}`}
    >
      {picture ? (
        <Image
          src={`/api/accounts/${accountId}/avatar?v=${encodeURIComponent(discord.avatar ?? '')}`}
          alt=""
          w={`${inner}px`}
          h={`${inner}px`}
          borderRadius="full"
          objectFit="cover"
        />
      ) : (
        <Box
          display="flex"
          alignItems="center"
          justifyContent="center"
          w={`${inner}px`}
          h={`${inner}px`}
          borderRadius="full"
          bg="#0e0f11"
          fontFamily="heading"
          fontSize={`${Math.round(size * 0.4)}px`}
          color="fg"
          aria-hidden
        >
          {initialOf(name)}
        </Box>
      )}
    </Flex>
  );
}
