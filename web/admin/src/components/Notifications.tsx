// The feedback toast of the design (A4, node 151:555): its icon, its title lit, a line of detail, the flare
// along its top; white for a success, silver for an information, red for an error.

import { Box, Flex, Portal, Text, Toast, Toaster } from '@chakra-ui/react';

import type { Tone } from '../live/state';
import {
  alertFlareGlow,
  alertFlareLine,
  flareGlow,
  flareLine,
  silverFlareGlow,
  silverFlareLine,
} from '../theme/system';
import { ToneIcon } from './EventFeed';
import { toaster } from './notify';

const tones: Record<string, Tone> = { info: 'info', success: 'bonus', error: 'alert', warning: 'alert' };

const looks: Record<
  Tone,
  { border: string; shadow: string; title: string; glow: string; line: string; lineGlow: string }
> = {
  bonus: {
    border: 'rgba(255,255,255,0.3)',
    shadow: '0 0 10px rgba(255,255,255,0.12)',
    title: 'fg',
    glow: '0 0 8px rgba(255,255,255,0.5)',
    line: flareLine,
    lineGlow: flareGlow,
  },
  info: {
    border: 'edge',
    shadow: '0 0 10px rgba(195,200,205,0.12)',
    title: 'fg.silver',
    glow: '0 0 8px rgba(195,200,205,0.5)',
    line: silverFlareLine,
    lineGlow: silverFlareGlow,
  },
  alert: {
    border: 'rgba(232,114,110,0.5)',
    shadow: '0 0 10px rgba(232,114,110,0.12)',
    title: 'alert',
    glow: '0 0 8px rgba(232,114,110,0.5)',
    line: alertFlareLine,
    lineGlow: alertFlareGlow,
  },
};

export function Notifications() {
  return (
    <Portal>
      <Toaster toaster={toaster} insetInline={{ mdDown: '4' }}>
        {(toast) => {
          const tone = tones[toast.type ?? 'info'] ?? 'info';
          const look = looks[tone];
          return (
            <Toast.Root
              position="relative"
              bg="bg"
              borderWidth="1px"
              borderColor={look.border}
              borderRadius="l2"
              boxShadow={look.shadow}
              w={{ base: '100%', md: '380px' }}
              px="16px"
              py="14px"
              display="flex"
              alignItems="flex-start"
              gap="14px"
              data-tone={tone}
            >
              <Box pt="1px">
                <ToneIcon tone={tone} />
              </Box>
              <Flex direction="column" gap="5px" minW={0} flex="1 0 0">
                <Toast.Title
                  fontFamily="heading"
                  fontWeight="normal"
                  fontSize="10px"
                  lineHeight="normal"
                  letterSpacing="2px"
                  textTransform="uppercase"
                  color={look.title}
                  textShadow={look.glow}
                >
                  {toast.title}
                </Toast.Title>
                {toast.description !== undefined && (
                  <Toast.Description asChild>
                    <Text textStyle="hint" color="fg.muted">
                      {toast.description}
                    </Text>
                  </Toast.Description>
                )}
              </Flex>
              <Box
                position="absolute"
                top="-1px"
                left="-1px"
                right="-1px"
                h="1px"
                opacity={0.6}
                bgImage={look.line}
                boxShadow={look.lineGlow}
              />
            </Toast.Root>
          );
        }}
      </Toaster>
    </Portal>
  );
}
