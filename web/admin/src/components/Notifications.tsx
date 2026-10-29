// The notification of the design (N1, node 44:27): its icon and its title lit, the flare under them, a line
// of detail; red for an alert.

import { Flex, Portal, Text, Toast, Toaster } from '@chakra-ui/react';

import type { Tone } from '../live/state';
import { alertFlareGlow, alertFlareLine, flareGlow, flareLine } from '../theme/system';
import { ToneIcon } from './EventFeed';
import { toaster } from './notify';

const tones: Record<string, Tone> = { info: 'info', success: 'bonus', error: 'alert', warning: 'alert' };

export function Notifications() {
  return (
    <Portal>
      <Toaster toaster={toaster} insetInline={{ mdDown: '4' }}>
        {(toast) => {
          const tone = tones[toast.type ?? 'info'] ?? 'info';
          const alert = tone === 'alert';
          return (
            <Toast.Root
              bg="rgba(0,0,0,0.85)"
              boxShadow="none"
              borderWidth="0"
              w="auto"
              minW="300px"
              px="0"
              py="8px"
              display="flex"
              flexDirection="column"
              alignItems="center"
              gap="6px"
              data-tone={tone}
            >
              <Flex align="center" gap="8px">
                <ToneIcon tone={tone} />
                <Toast.Title
                  fontFamily="heading"
                  fontWeight="normal"
                  fontSize="11px"
                  letterSpacing="2.4px"
                  textTransform="uppercase"
                  color={alert ? 'alert' : 'fg'}
                  textShadow={alert ? '0 0 8px rgba(232,114,110,0.6)' : '0 0 8px rgba(255,255,255,0.6)'}
                >
                  {toast.title}
                </Toast.Title>
              </Flex>
              <Flex
                h="1px"
                w="300px"
                bgImage={alert ? alertFlareLine : flareLine}
                boxShadow={alert ? alertFlareGlow : flareGlow}
              />
              {toast.description !== undefined && (
                <Toast.Description asChild>
                  <Text fontSize="10px" color="fg.muted" textAlign="center">
                    {toast.description}
                  </Text>
                </Toast.Description>
              )}
            </Toast.Root>
          );
        }}
      </Toaster>
    </Portal>
  );
}
