// The one WebSocket of the panel, /api/events: opened once, reconnected with a growing delay, its frames
// applied to the state that every page reads. A reconnection gets a new snapshot, so nothing is missed.

import { ApiError, api } from '../api/client';
import type { LiveEvent } from '../api/types';
import { applyEvent, initialState, type LiveState } from './state';

/** The delays before each new attempt, the last one repeated. */
export const ReconnectDelays = [1000, 2000, 5000, 10000, 30000];

export interface StoreOptions {
  /** Opens the WebSocket: the browser's, or a fake in the tests. */
  open?: (url: string) => WebSocket;
  /** Whether the session still holds, asked when a connection fails before it opened. */
  hasSession?: () => Promise<boolean>;
  now?: () => number;
}

async function sessionHolds(): Promise<boolean> {
  try {
    await api.server();
    return true;
  } catch (error) {
    return !(error instanceof ApiError && error.signedOut);
  }
}

function eventsUrl(): string {
  const scheme = window.location.protocol === 'https:' ? 'wss' : 'ws';
  return `${scheme}://${window.location.host}/api/events`;
}

export class LiveStore {
  private state: LiveState = initialState;
  private readonly listeners = new Set<() => void>();
  private socket: WebSocket | null = null;
  private timer: ReturnType<typeof setTimeout> | null = null;
  private attempts = 0;
  private stopped = true;
  private readonly open: (url: string) => WebSocket;
  private readonly hasSession: () => Promise<boolean>;
  private readonly now: () => number;

  constructor(options: StoreOptions = {}) {
    this.open = options.open ?? ((url) => new WebSocket(url));
    this.hasSession = options.hasSession ?? sessionHolds;
    this.now = options.now ?? Date.now;
  }

  readonly subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  readonly getSnapshot = (): LiveState => this.state;

  start(): void {
    if (!this.stopped) return;
    this.stopped = false;
    this.connect();
  }

  stop(): void {
    this.stopped = true;
    if (this.timer !== null) clearTimeout(this.timer);
    this.timer = null;
    const socket = this.socket;
    this.socket = null;
    socket?.close();
  }

  /** Applies a frame as if it came from the WebSocket: the tests, and the stories of the components. */
  receive(event: LiveEvent): void {
    this.set(applyEvent(this.state, event, this.now()));
  }

  private set(state: LiveState): void {
    this.state = state;
    for (const listener of this.listeners) listener();
  }

  private connect(): void {
    let opened = false;
    const socket = this.open(eventsUrl());
    this.socket = socket;
    socket.onopen = () => {
      opened = true;
      this.attempts = 0;
      this.set({ ...this.state, connection: 'open' });
    };
    socket.onmessage = (message: MessageEvent) => {
      if (typeof message.data !== 'string') return;
      let event: LiveEvent;
      try {
        event = JSON.parse(message.data) as LiveEvent;
      } catch {
        return;
      }
      this.receive(event);
    };
    socket.onclose = () => {
      if (this.socket !== socket || this.stopped) return;
      this.socket = null;
      // Refused before it opened: the session may have expired, which no new attempt fixes
      if (!opened) {
        void this.hasSession().then((holds) => {
          if (this.stopped) return;
          if (holds) {
            this.retry();
          } else {
            this.set({ ...this.state, connection: 'signed-out' });
          }
        });
        return;
      }
      this.retry();
    };
  }

  private retry(): void {
    const delay = ReconnectDelays[Math.min(this.attempts, ReconnectDelays.length - 1)] ?? 30000;
    this.attempts += 1;
    this.set({ ...this.state, connection: 'reconnecting' });
    this.timer = setTimeout(() => {
      this.timer = null;
      if (!this.stopped) this.connect();
    }, delay);
  }
}
