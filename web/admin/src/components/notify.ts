import { createToaster } from '@chakra-ui/react';

import type { Tone } from '../live/state';

/** The notifications of the panel, at the top of the page, centred, as the design's N1: under its header. */
export const toaster = createToaster({
  placement: 'top',
  max: 4,
  overlap: false,
  gap: 12,
  offsets: { top: '80px', right: '16px', bottom: '16px', left: '16px' },
});

const types: Record<Tone, 'info' | 'success' | 'error'> = { info: 'info', bonus: 'success', alert: 'error' };

/** Tells the host what happened: a title, and a line of detail. */
export function notify(tone: Tone, title: string, detail?: string): void {
  toaster.create({ type: types[tone], title, description: detail, duration: tone === 'alert' ? 8000 : 4000 });
}
