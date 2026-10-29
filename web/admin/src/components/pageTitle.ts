// The title of the page, which each page gives the top bar of the shell.

import { createContext, useContext, useEffect } from 'react';

export const PageTitleContext = createContext<(title: string) => void>(() => undefined);

/** Gives the shell the title of the page. */
export function usePageTitle(title: string): void {
  const set = useContext(PageTitleContext);
  useEffect(() => {
    set(title);
  }, [set, title]);
}
