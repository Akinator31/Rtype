// The games and the players over time, since the panel opened: two lines of light on the rail of the gauges,
// the games lit, the players silver.

import { Box, Flex, Text } from '@chakra-ui/react';

import { clock } from '../format';
import type { Sample } from '../live/state';
import { strings } from '../strings';

const Width = 600;
const Height = 140;

function path(samples: Sample[], value: (sample: Sample) => number, top: number, from: number, span: number) {
  return samples
    .map((sample, index) => {
      const x = span === 0 ? Width : ((sample.at - from) / span) * Width;
      const y = Height - (value(sample) / top) * (Height - 8);
      return `${index === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');
}

export function HistoryChart({ samples, empty, label }: { samples: Sample[]; empty: string; label: string }) {
  if (samples.length < 2) {
    return (
      <Text color="fg.subtle" fontSize="10px">
        {empty}
      </Text>
    );
  }
  const first = samples[0] as Sample;
  const last = samples[samples.length - 1] as Sample;
  const top = Math.max(1, ...samples.map((sample) => Math.max(sample.games, sample.players)));
  const span = last.at - first.at;
  // Each value holds until the next: a step at each change
  const steps = samples.flatMap((sample, index) => {
    const next = samples[index + 1];
    return next === undefined ? [sample] : [sample, { ...sample, at: next.at }];
  });
  return (
    <Box>
      <Box
        as="svg"
        role="img"
        aria-label={strings.overview.historyLabel(label, last.games, top)}
        {...{ viewBox: `0 0 ${Width} ${Height}`, preserveAspectRatio: 'none' }}
        w="100%"
        h="140px"
        display="block"
        overflow="visible"
      >
        <line
          x1="0"
          y1={Height}
          x2={Width}
          y2={Height}
          stroke="var(--chakra-colors-rail)"
          strokeWidth="1"
          vectorEffect="non-scaling-stroke"
        />
        <path
          d={path(steps, (sample) => sample.players, top, first.at, span)}
          fill="none"
          stroke="var(--chakra-colors-fg-muted)"
          strokeWidth="1"
          vectorEffect="non-scaling-stroke"
        />
        <path
          d={path(steps, (sample) => sample.games, top, first.at, span)}
          fill="none"
          stroke="var(--chakra-colors-fg)"
          strokeWidth="1.5"
          vectorEffect="non-scaling-stroke"
          style={{ filter: 'drop-shadow(0 0 4px rgba(255,255,255,0.8))' }}
        />
      </Box>
      <Flex justify="space-between" mt="8px" textStyle="caption" color="fg.subtle">
        <span>{clock(first.at)}</span>
        <span>{clock(last.at)}</span>
      </Flex>
      <Flex justify="center" gap="16px" wrap="wrap" mt="8px" textStyle="caption" color="fg.subtle">
        <Text as="span" color="fg">
          — {strings.overview.games}
        </Text>
        <Text as="span" color="fg.muted">
          — {strings.overview.players}
        </Text>
        <span>{strings.overview.most(top)}</span>
      </Flex>
    </Box>
  );
}
