import { ChakraProvider } from '@chakra-ui/react';
import { useState } from 'react';
import { RouterProvider, createBrowserRouter } from 'react-router';

import { LiveProvider } from './live/LiveProvider';
import { LiveStore } from './live/store';
import { routes } from './routes';
import { system } from './theme/system';

export function App() {
  const [store] = useState(() => new LiveStore());
  const [router] = useState(() => createBrowserRouter(routes));
  return (
    <ChakraProvider value={system}>
      <LiveProvider store={store}>
        <RouterProvider router={router} />
      </LiveProvider>
    </ChakraProvider>
  );
}
