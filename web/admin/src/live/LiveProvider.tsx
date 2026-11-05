import { useEffect, type ReactNode } from 'react';

import { LiveContext } from './context';
import type { LiveStore } from './store';

/** Gives the store to the pages, and keeps its WebSocket open while the panel is shown. */
export function LiveProvider({ store, children }: { store: LiveStore; children: ReactNode }) {
  useEffect(() => {
    store.start();
    return () => store.stop();
  }, [store]);
  return <LiveContext.Provider value={store}>{children}</LiveContext.Provider>;
}
