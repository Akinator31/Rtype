// The pieces every screen of the design is built from: its cards (a title in chrome, a hint or a link on the
// right), the indicators of the overview, the pairs of a label and its value, the tool bar above a table, the
// title of the page in the top bar.

import { Box, Flex, Input, Text, type BoxProps, type FlexProps } from '@chakra-ui/react';
import type { ReactNode } from 'react';
import { Link } from 'react-router';

import { glow } from '../theme/system';

/** A card of a screen: its title in chrome, what it reads or a link on the right. */
export function Card({
  title,
  hint,
  link,
  aside,
  children,
  ...props
}: FlexProps & {
  title?: string;
  hint?: ReactNode;
  link?: { to: string; label: string };
  aside?: ReactNode;
}) {
  return (
    <Flex
      as="section"
      layerStyle="card"
      direction="column"
      gap="12px"
      px="24px"
      py="22px"
      minW={0}
      aria-label={title}
      {...props}
    >
      {(title !== undefined || hint !== undefined || link !== undefined || aside !== undefined) && (
        <Flex align="center" justify="space-between" gap="16px" minH="14px">
          {title !== undefined && (
            <Text as="h2" textStyle="cardTitle">
              {title}
            </Text>
          )}
          {hint !== undefined && (
            <Text textStyle="hint" color="fg.subtle" textAlign="right" truncate>
              {hint}
            </Text>
          )}
          {link !== undefined && (
            <Text
              asChild
              textStyle="link"
              color="fg.silver"
              textDecoration="none"
              _hover={{ color: 'fg', textShadow: glow(0.6) }}
            >
              <Link to={link.to}>{link.label}</Link>
            </Text>
          )}
          {aside}
        </Flex>
      )}
      {children}
    </Flex>
  );
}

/** An indicator of the overview: its label, its number lit, a line under it; the page it opens. */
export function Stat({
  label,
  value,
  detail,
  tone = 'info',
  to,
  frozen = false,
}: {
  label: string;
  value: ReactNode;
  detail?: ReactNode;
  tone?: 'info' | 'alert';
  to?: string;
  /** The last value known, the master silent: dimmed. */
  frozen?: boolean;
}) {
  const content = (
    <>
      <Text textStyle="label" color="fg.subtle" truncate>
        {label}
      </Text>
      <Text
        fontFamily="heading"
        fontSize="26px"
        lineHeight="normal"
        letterSpacing="1px"
        textTransform="uppercase"
        textShadow="0 0 14px rgba(255,255,255,0.45)"
        whiteSpace="nowrap"
      >
        {value}
      </Text>
      {detail !== undefined && (
        <Text textStyle="hint" color={tone === 'alert' ? 'alert' : 'fg.muted'} truncate>
          {detail}
        </Text>
      )}
    </>
  );
  const look = {
    layerStyle: 'card',
    direction: 'column' as const,
    gap: '8px',
    px: '22px',
    py: '20px',
    minW: 0,
    flex: '1 1 0',
    opacity: frozen ? 0.45 : 1,
    'aria-label': label,
  };
  if (to === undefined) {
    return (
      <Flex role="group" {...look}>
        {content}
      </Flex>
    );
  }
  return (
    <Flex
      asChild
      {...look}
      textDecoration="none"
      transition="border-color 120ms"
      _hover={{ borderColor: 'edge' }}
      _focusVisible={{ outline: 'none', borderColor: 'rgba(255,255,255,0.45)' }}
    >
      <Link to={to}>{content}</Link>
    </Flex>
  );
}

/** A label and its value, on a line of a card; a value of the server lit in Audiowide. */
export function Pair({
  label,
  children,
  plain = false,
}: {
  label: string;
  children: ReactNode;
  /** A value in the font of the text, such as a date. */
  plain?: boolean;
}) {
  return (
    <Flex
      align="center"
      justify="space-between"
      gap="16px"
      pb="10px"
      borderBottomWidth="1px"
      borderColor="border"
      minH="26px"
    >
      <Text textStyle="body" color="fg.muted" whiteSpace="nowrap">
        {label}
      </Text>
      {plain ? (
        <Text textStyle="body" color="fg.silver" textAlign="right" truncate>
          {children}
        </Text>
      ) : (
        <Box textStyle="number" color="fg" textShadow={glow(0.35)} textAlign="right" minW={0} truncate>
          {children}
        </Box>
      )}
    </Flex>
  );
}

/** What a table or a list shows when it has nothing. */
export function Empty({ children }: { children: ReactNode }) {
  return (
    <Text textStyle="body" color="fg.subtle" py="24px" textAlign="center">
      {children}
    </Text>
  );
}

/** The bar above a table: its search, its filters, then what it does on the right. */
export function Toolbar({ children, end }: { children: ReactNode; end?: ReactNode }) {
  return (
    <Flex align="center" gap="12px" wrap="wrap">
      {children}
      <Box flex="1 0 0" minW="8px" />
      {end}
    </Flex>
  );
}

export function SearchField({
  label,
  value,
  onChange,
  width = '300px',
}: {
  label: string;
  value: string;
  onChange: (value: string) => void;
  width?: string;
}) {
  return (
    <Input
      type="search"
      placeholder={label}
      aria-label={label}
      value={value}
      onChange={(event) => onChange(event.target.value)}
      w={{ base: '100%', md: width }}
      h="38px"
      autoComplete="off"
    />
  );
}

/** A note under a block: grey, small. */
export function Note({ children, ...props }: BoxProps & { children: ReactNode }) {
  return (
    <Box textStyle="hint" color="fg.subtle" lineHeight="16px" {...props}>
      {children}
    </Box>
  );
}
