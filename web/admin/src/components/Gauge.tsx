// The gauge of the design (J1, node 42:177): a rail, the light that fills it from the left, and a sparkle
// where the light ends; brighter and thicker once full.

import { Box, Image } from '@chakra-ui/react';

import sparkle from '../assets/flares/gauge.svg';
import fullSparkle from '../assets/flares/gauge-full.svg';

export interface GaugeProps {
  /** How full, from 0 to 1. */
  value: number;
  /** What the gauge measures, for the readers of the screen. */
  label: string;
  /** The value as words, such as "3 of 4". */
  valueText?: string;
  width?: string;
}

export function Gauge({ value, label, valueText, width = '100%' }: GaugeProps) {
  const ratio = Number.isFinite(value) ? Math.min(1, Math.max(0, value)) : 0;
  const full = ratio >= 1;
  const percent = `${ratio * 100}%`;
  return (
    <Box
      role="meter"
      aria-label={label}
      aria-valuemin={0}
      aria-valuemax={100}
      aria-valuenow={Math.round(ratio * 100)}
      aria-valuetext={valueText}
      position="relative"
      h="20px"
      w={width}
      minW="60px"
    >
      <Box position="absolute" left="0" right="0" top="10px" h="1px" bg="rail" />
      {ratio > 0 && (
        <>
          <Box
            position="absolute"
            left="0"
            top={full ? '9px' : '9.5px'}
            h={full ? '2px' : '1.5px'}
            w={percent}
            bgImage="linear-gradient(90deg, rgba(255,255,255,0) 0%, #ffffff 100%)"
            boxShadow={full ? '0 0 10px 0 rgba(255,255,255,0.9)' : '0 0 5px 0 rgba(255,255,255,0.6)'}
          />
          <Image
            src={full ? fullSparkle : sparkle}
            alt=""
            position="absolute"
            left={percent}
            top="10px"
            w={full ? '50px' : '38.86px'}
            h={full ? '27.9px' : '24px'}
            maxW="none"
            transform="translate(-50%, -50%)"
            pointerEvents="none"
          />
        </>
      )}
    </Box>
  );
}
