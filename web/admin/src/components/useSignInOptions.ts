// What the master offers to open a session besides the local token: an account and its password, Discord.

import { useEffect, useState } from 'react';

import { api } from '../api/client';
import type { SignInOptions } from '../api/types';

/** What the master offers besides the local token; nothing until it answered, or if it cannot. */
export function useSignInOptions(): SignInOptions | null {
  const [options, setOptions] = useState<SignInOptions | null>(null);
  useEffect(() => {
    let current = true;
    api
      .signInOptions()
      .then((answer) => current && setOptions(answer))
      .catch(() => undefined);
    return () => {
      current = false;
    };
  }, []);
  return options;
}
