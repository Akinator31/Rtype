// The banner of the design (N3, node 44:99), for a major event: a veil across the page between two lines of
// light, its title in chrome (red for an alert) between two sparkles, a line of detail under it.

import { Box, Flex, Image, Text } from '@chakra-ui/react';

import alertSparkle from '../assets/flares/banner-alert.svg';
import sparkle from '../assets/flares/banner.svg';
import { alertFlareGlow, chromeGradient, flareGlow } from '../theme/system';

export interface BannerProps {
  title: string;
  detail?: string;
  tone?: 'info' | 'alert';
}

const lineOf = (alert: boolean) =>
  alert
    ? 'linear-gradient(90deg, rgba(232,114,110,0) 0%, rgba(232,114,110,0.7) 50%, rgba(232,114,110,0) 100%)'
    : 'linear-gradient(90deg, rgba(255,255,255,0) 0%, rgba(255,255,255,0.7) 50%, rgba(255,255,255,0) 100%)';

function Sparkle({ alert }: { alert: boolean }) {
  return (
    <Box position="relative" w="26px" h="9px" flexShrink={0}>
      <Image
        src={alert ? alertSparkle : sparkle}
        alt=""
        position="absolute"
        top="50%"
        left="50%"
        w="41.71px"
        h="25px"
        maxW="none"
        transform="translate(-50%, -50%)"
      />
    </Box>
  );
}

export function Banner({ title, detail, tone = 'info' }: BannerProps) {
  const alert = tone === 'alert';
  return (
    <Box
      role={alert ? 'alert' : 'status'}
      position="relative"
      h="64px"
      w="100%"
      bgImage={
        alert
          ? 'linear-gradient(90deg, rgba(232,114,110,0) 0%, rgba(232,114,110,0.07) 50%, rgba(232,114,110,0) 100%)'
          : 'linear-gradient(90deg, rgba(255,255,255,0) 0%, rgba(255,255,255,0.07) 50%, rgba(255,255,255,0) 100%)'
      }
      bgSize="100% 60px"
      backgroundPosition="center"
      bgRepeat="no-repeat"
      data-tone={tone}
    >
      <Box
        position="absolute"
        top="0"
        left="0"
        right="0"
        h="1px"
        bgImage={lineOf(alert)}
        boxShadow={alert ? alertFlareGlow : flareGlow}
      />
      <Box
        position="absolute"
        bottom="0"
        left="0"
        right="0"
        h="1px"
        bgImage={lineOf(alert)}
        boxShadow={alert ? alertFlareGlow : flareGlow}
      />
      <Flex direction="column" align="center" justify="center" h="100%" gap="6px" px="16px">
        <Flex align="center" gap="14px">
          <Sparkle alert={alert} />
          <Text
            fontFamily="heading"
            fontSize={{ base: '14px', md: '18px' }}
            letterSpacing={{ base: '3px', md: '6px' }}
            textTransform="uppercase"
            whiteSpace="nowrap"
            {...(alert
              ? { color: 'alert', textShadow: '0 0 14px rgba(232,114,110,0.6)' }
              : {
                  bgImage: chromeGradient,
                  bgClip: 'text',
                  color: 'transparent',
                  filter: 'drop-shadow(0 0 14px rgba(255,255,255,0.6))',
                })}
          >
            {title}
          </Text>
          <Sparkle alert={alert} />
        </Flex>
        {detail !== undefined && (
          <Text
            fontSize="9px"
            letterSpacing="2.4px"
            textTransform="uppercase"
            color="fg.muted"
            textAlign="center"
          >
            {detail}
          </Text>
        )}
      </Flex>
    </Box>
  );
}
