import { useEffect, useState } from 'react';

/** The time now, renewed every `interval` milliseconds: for ages and uptimes that count by themselves. */
export function useNow(interval = 1000): number {
  const [now, setNow] = useState(() => Date.now());
  useEffect(() => {
    const timer = setInterval(() => setNow(Date.now()), interval);
    return () => clearInterval(timer);
  }, [interval]);
  return now;
}
