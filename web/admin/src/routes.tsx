import type { RouteObject } from 'react-router';

import { Actions } from './pages/Actions';
import { GamePage } from './pages/GamePage';
import { NotFound } from './pages/NotFound';
import { Games } from './pages/Games';
import { Overview } from './pages/Overview';
import { Players } from './pages/Players';
import { Shell } from './Shell';

/** The pages of the panel; the master serves index.html for each of their addresses. */
export const routes: RouteObject[] = [
  {
    path: '/',
    element: <Shell />,
    children: [
      { index: true, element: <Overview /> },
      { path: 'games', element: <Games /> },
      { path: 'games/:id', element: <GamePage /> },
      { path: 'players', element: <Players /> },
      { path: 'actions', element: <Actions /> },
      { path: '*', element: <NotFound /> },
    ],
  },
];
