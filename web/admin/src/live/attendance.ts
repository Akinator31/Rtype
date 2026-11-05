// The attendance of the overview: the most players of each minute of the last hour, from the samples.

import type { Sample } from './state';

export const Bars = 60;
export const BarLength = 60_000;

/** The most players of each minute of the last hour, the oldest first; null for a minute not seen. */
export function attendance(samples: Sample[], now: number): (number | null)[] {
  const bars: (number | null)[] = Array.from({ length: Bars }, () => null);
  const start = now - Bars * BarLength;
  samples.forEach((sample, index) => {
    // A sample holds until the next one
    const until = samples[index + 1]?.at ?? now;
    if (until < start) return;
    // A sample a little after `now`, taken between two ticks of the clock, counts for now
    const first = Math.min(Bars - 1, Math.max(0, Math.floor((sample.at - start) / BarLength)));
    const last = Math.min(Bars - 1, Math.floor((until - start) / BarLength));
    for (let bar = first; bar <= last; bar++) {
      bars[bar] = Math.max(bars[bar] ?? 0, sample.players);
    }
  });
  return bars;
}
