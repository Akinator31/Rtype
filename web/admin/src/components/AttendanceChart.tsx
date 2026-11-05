// The attendance of the overview (node 146:143, "Fréquentation"): a bar a minute of the players connected,
// the most of each minute, over the last hour since the panel opened; the bar of now lit.

import { Box, Flex, Text } from '@chakra-ui/react';

import { attendance, Bars } from '../live/attendance';
import type { Sample } from '../live/state';
import { strings } from '../strings';

export function AttendanceChart({ samples, now }: { samples: Sample[]; now: number }) {
  const bars = attendance(samples, now);
  const seen = bars.filter((bar): bar is number => bar !== null);
  if (seen.length === 0) {
    return (
      <Text color="fg.subtle" fontSize="10px" flex="1 0 0">
        {strings.overview.attendanceEmpty}
      </Text>
    );
  }
  const most = Math.max(1, ...seen);
  const last = seen[seen.length - 1] ?? 0;
  return (
    <Flex direction="column" gap="16px" flex="1 0 0" minH="140px">
      <Flex
        role="img"
        aria-label={strings.overview.attendanceLabel(last, most)}
        align="flex-end"
        gap={{ base: '2px', md: '4px' }}
        flex="1 0 0"
        minH="100px"
      >
        {bars.map((bar, index) => {
          const current = index === Bars - 1;
          return (
            <Box
              key={index}
              flex="1 1 0"
              minW="2px"
              h={bar === null || bar === 0 ? '0' : `${Math.max(4, (bar / most) * 100)}%`}
              borderRadius="1px"
              bgImage={
                current
                  ? 'linear-gradient(180deg, rgba(255,255,255,0.95) 0%, rgba(255,255,255,0.04) 100%)'
                  : 'linear-gradient(180deg, rgba(255,255,255,0.5) 0%, rgba(255,255,255,0.04) 100%)'
              }
              boxShadow={current ? '0 0 10px 0 rgba(255,255,255,0.6)' : undefined}
              data-players={bar ?? undefined}
            />
          );
        })}
      </Flex>
      <Flex justify="space-between" textStyle="label" fontSize="9px" letterSpacing="1.4px" color="fg.group">
        {strings.overview.attendanceAxis.map((label) => (
          <span key={label}>{label}</span>
        ))}
      </Flex>
    </Flex>
  );
}
