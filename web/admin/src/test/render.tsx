// The panel in a test: its pages at an address, its theme, and its store on a WebSocket the test plays.

import { ChakraProvider } from '@chakra-ui/react';
import { act, render } from '@testing-library/react';
import { RouterProvider, createMemoryRouter } from 'react-router';

import type { LiveEvent } from '../api/types';
import { LiveProvider } from '../live/LiveProvider';
import { LiveStore } from '../live/store';
import { routes } from '../routes';
import { system } from '../theme/system';

/** A WebSocket the test opens, sends frames on, and closes. */
export class FakeSocket {
  static opened: FakeSocket[] = [];
  readonly url: string;
  onopen: (() => void) | null = null;
  onmessage: ((message: { data: unknown }) => void) | null = null;
  onclose: (() => void) | null = null;
  closed = false;

  constructor(url: string) {
    this.url = url;
    FakeSocket.opened.push(this);
  }

  open(): void {
    this.onopen?.();
  }

  send(event: LiveEvent): void {
    this.onmessage?.({ data: JSON.stringify(event) });
  }

  close(): void {
    if (this.closed) return;
    this.closed = true;
    this.onclose?.();
  }

  static last(): FakeSocket {
    const socket = FakeSocket.opened[FakeSocket.opened.length - 1];
    if (socket === undefined) throw new Error('No WebSocket was opened');
    return socket;
  }
}

export function fakeStore(options: { hasSession?: () => Promise<boolean> } = {}): LiveStore {
  FakeSocket.opened = [];
  return new LiveStore({
    open: (url) => new FakeSocket(url) as unknown as WebSocket,
    hasSession: options.hasSession ?? (() => Promise.resolve(true)),
  });
}

/**
 * Renders the panel at `path`, its WebSocket opened and sent this snapshot and these events; or refused, as
 * the master refuses a page without a session, when `signedOut`.
 */
export function renderPanel(
  path: string,
  events: LiveEvent[] = [],
  options: { signedOut?: boolean; hasSession?: () => Promise<boolean> } = {},
) {
  const store = fakeStore({
    hasSession: options.hasSession ?? (() => Promise.resolve(options.signedOut !== true)),
  });
  const router = createMemoryRouter(routes, { initialEntries: [path] });
  const view = render(
    <ChakraProvider value={system}>
      <LiveProvider store={store}>
        <RouterProvider router={router} />
      </LiveProvider>
    </ChakraProvider>,
  );
  const socket = FakeSocket.last();
  act(() => {
    if (options.signedOut === true) {
      socket.close();
      return;
    }
    socket.open();
    for (const event of events) socket.send(event);
  });
  /** Sends a frame on the WebSocket, as the master would. */
  const push = (event: LiveEvent) => act(() => socket.send(event));
  return { ...view, store, router, socket, push };
}
