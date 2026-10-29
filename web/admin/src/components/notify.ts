import { createToaster } from '@chakra-ui/react';

import type { Tone } from '../live/state';

/** The feedback toasts of the panel (A4): top right, under the top bar, 5 seconds. */
export const toaster = createToaster({
  placement: 'top-end',
  max: 4,
  overlap: false,
  gap: 12,
  offsets: { top: '100px', right: '24px', bottom: '16px', left: '16px' },
});

const types: Record<Tone, 'info' | 'success' | 'error'> = { info: 'info', bonus: 'success', alert: 'error' };

/** Tells the host what happened: success, information or error, a title and a line of detail. */
export function notify(tone: Tone, title: string, detail?: string): void {
  toaster.create({ type: types[tone], title, description: detail, duration: tone === 'alert' ? 8000 : 5000 });
}
