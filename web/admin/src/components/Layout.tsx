// The pieces every page is built from: its header after the headers of the design's blocks (a small label, a
// title in chrome, a line of description), its surfaces, and the numbers of a stat.

import { Box, Flex, Heading, Text, type BoxProps } from '@chakra-ui/react';
import type { ReactNode } from 'react';

export function PageHeader({
  caption,
  title,
  description,
  actions,
}: {
  caption: string;
  title: string;
  description?: string;
  actions?: ReactNode;
}) {
  return (
    <Flex
      justify="space-between"
      align={{ base: 'flex-start', md: 'flex-end' }}
      gap="16px"
      wrap="wrap"
      mb="24px"
    >
      <Flex direction="column" gap="16px" minW={0}>
        <Flex align="center" gap="12px">
          <Text textStyle="caption" color="fg.muted">
            {caption}
          </Text>
          <Heading as="h1" textStyle="chrome" fontWeight="normal">
            {title}
          </Heading>
        </Flex>
        {description !== undefined && <Text color="fg.muted">{description}</Text>}
      </Flex>
      {actions !== undefined && (
        <Flex gap="8px" wrap="wrap">
          {actions}
        </Flex>
      )}
    </Flex>
  );
}

/** A block of the page on its surface; its title, if given, a small label. */
export function Surface({ title, children, ...props }: BoxProps & { title?: string }) {
  return (
    <Box layerStyle="surface" p="24px" minW={0} {...props}>
      {title !== undefined && (
        <Text as="h2" textStyle="caption" color="fg.muted" mb="16px">
          {title}
        </Text>
      )}
      {children}
    </Box>
  );
}

/** A number of the server, large, in the font of the titles. */
export function Stat({ label, value, children }: { label: string; value: ReactNode; children?: ReactNode }) {
  return (
    <Surface>
      <Text textStyle="caption" color="fg.muted">
        {label}
      </Text>
      <Text textStyle="number" fontSize="28px" mt="8px" textShadow="0 0 12px rgba(255,255,255,0.35)">
        {value}
      </Text>
      {children}
    </Surface>
  );
}

/** A label and its value, on a line. */
export function Field({ label, children }: { label: string; children: ReactNode }) {
  return (
    <Flex justify="space-between" gap="16px" py="6px" borderBottomWidth="1px" borderColor="border">
      <Text textStyle="caption" color="fg.muted">
        {label}
      </Text>
      <Text textStyle="number" fontSize="12px" textAlign="right">
        {children}
      </Text>
    </Flex>
  );
}

/** What a table shows when it has no row. */
export function Empty({ children }: { children: ReactNode }) {
  return (
    <Text color="fg.subtle" py="24px" textAlign="center">
      {children}
    </Text>
  );
}
