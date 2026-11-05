import { Navigate, type RouteObject } from 'react-router';

import { Accounts } from './pages/Accounts';
import { Bans } from './pages/Bans';
import { GamePage } from './pages/GamePage';
import { Games } from './pages/Games';
import { History } from './pages/History';
import { Messages } from './pages/Messages';
import { NotFound } from './pages/NotFound';
import { Overview } from './pages/Overview';
import { Players } from './pages/Players';
import { Settings } from './pages/Settings';
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
      { path: 'bans', element: <Bans /> },
      { path: 'messages', element: <Messages /> },
      { path: 'history', element: <History /> },
      // The record had this address before the history
      { path: 'actions', element: <Navigate to="/history" replace /> },
      { path: 'accounts', element: <Accounts /> },
      { path: 'settings', element: <Settings /> },
      { path: '*', element: <NotFound /> },
    ],
  },
];
