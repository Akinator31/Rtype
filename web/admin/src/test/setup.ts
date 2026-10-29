import '@testing-library/jest-dom/vitest';

import { cleanup } from '@testing-library/react';
import { afterAll, afterEach, beforeAll } from 'vitest';

import { resetSession } from '../api/client';
import { toaster } from '../components/notify';
import { sent, server } from './server';

// What jsdom lacks and Chakra UI asks for
window.matchMedia ??= ((query: string) => ({
  matches: false,
  media: query,
  onchange: null,
  addListener: () => undefined,
  removeListener: () => undefined,
  addEventListener: () => undefined,
  removeEventListener: () => undefined,
  dispatchEvent: () => false,
})) as typeof window.matchMedia;
globalThis.ResizeObserver ??= class {
  observe() {}
  unobserve() {}
  disconnect() {}
};
window.scrollTo = () => undefined;
Element.prototype.scrollTo = () => undefined;

beforeAll(() => server.listen({ onUnhandledFrame: 'error' }));
afterEach(() => {
  cleanup();
  // One toaster for the whole panel: the notifications of a test would hold the places of the next
  toaster.remove();
  server.resetHandlers();
  sent.length = 0;
  resetSession();
});
afterAll(() => server.close());
