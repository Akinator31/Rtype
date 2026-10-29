import { createContext, useContext, useSyncExternalStore } from 'react';

import type { LiveState } from './state';
import type { LiveStore } from './store';

export const LiveContext = createContext<LiveStore | null>(null);

/** The live state of the server: the component renders again on each frame of the WebSocket. */
export function useLive(): LiveState {
  const store = useContext(LiveContext);
  if (store === null) {
    throw new Error('useLive() needs a LiveProvider');
  }
  return useSyncExternalStore(store.subscribe, store.getSnapshot);
}

/** The store itself, for what acts on the connection: try again now. */
export function useLiveStore(): LiveStore {
  const store = useContext(LiveContext);
  if (store === null) {
    throw new Error('useLiveStore() needs a LiveProvider');
  }
  return store;
}
