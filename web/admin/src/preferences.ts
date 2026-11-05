// What the host chose for this panel, kept in this browser only: confirm the actions, the sound of a stuck
// instance, the templates of the messages. Without a storage (a private window), the defaults hold.

import { useSyncExternalStore } from 'react';

export interface Preferences {
  confirmActions: boolean;
  soundAlert: boolean;
  templates: string[];
}

export const defaultPreferences: Preferences = {
  confirmActions: true,
  soundAlert: false,
  templates: ['Restart in 5 minutes', 'Maintenance at 23:00, finish your games', 'Keep it friendly'],
};

const Key = 'rtype-panel-preferences';
const listeners = new Set<() => void>();
let current: Preferences | null = null;

function load(): Preferences {
  try {
    const stored = window.localStorage.getItem(Key);
    if (stored === null) return defaultPreferences;
    const parsed = JSON.parse(stored) as Partial<Preferences>;
    return {
      confirmActions:
        typeof parsed.confirmActions === 'boolean'
          ? parsed.confirmActions
          : defaultPreferences.confirmActions,
      soundAlert: typeof parsed.soundAlert === 'boolean' ? parsed.soundAlert : defaultPreferences.soundAlert,
      templates: Array.isArray(parsed.templates)
        ? parsed.templates.filter((text): text is string => typeof text === 'string')
        : defaultPreferences.templates,
    };
  } catch {
    return defaultPreferences;
  }
}

export function preferences(): Preferences {
  current ??= load();
  return current;
}

export function setPreferences(change: Partial<Preferences>): void {
  current = { ...preferences(), ...change };
  try {
    window.localStorage.setItem(Key, JSON.stringify(current));
  } catch {
    // Kept for this page only
  }
  for (const listener of listeners) listener();
}

/** Forgets what was chosen, for the tests. */
export function resetPreferences(): void {
  current = null;
  try {
    window.localStorage.removeItem(Key);
  } catch {
    // Nothing kept
  }
}

function subscribe(listener: () => void): () => void {
  listeners.add(listener);
  return () => listeners.delete(listener);
}

export function usePreferences(): Preferences {
  return useSyncExternalStore(subscribe, preferences);
}
