# The administration panel

The web panel of an R-Type server: it shows the server live, its games, its players and the record of the
actions, and acts on them. It is served by the master itself, from `admin/` beside `r-type_server`, and talks to
the [administration API](../../docs/protocol/admin-api.md) of that master only. How it is built:
[The panel](../../docs/architecture/panel.md); how a host uses it:
[Administer a server](../../docs/guides/administer-a-server.md).

Vite, React 19, TypeScript (strict), Chakra UI v3, React Router; ESLint and Prettier; Vitest, Testing Library and
MSW; Playwright.

## Design

The reference is the Figma design of the panel, the frame **"R-Type UI — Éléments retenus"** (node `42:3`),
direction "Éclat stellaire":
<https://www.figma.com/design/RrrH0ic4ttdyDoc3nRnWBr/Untitled?node-id=42-3>

Black and silver, light as the only accent, red for alerts; Audiowide for titles, labels, buttons and numbers,
Inter for text. Every value of the design is set once, in `src/theme/system.ts` (tokens, text styles, recipes),
and never in a page. The elements of the design each have their component:

| Design                                          | Component                                           |
| ----------------------------------------------- | --------------------------------------------------- |
| Menu button (Normal, Survol, Pressé, Désactivé) | `FlareButton`, `FlareLink` (`components/Flare.tsx`) |
| J1, Jauge                                       | `Gauge`                                             |
| N1, Notification                                | `Notifications`, `notify()`                         |
| N3, Bandeau traversant                          | `Banner`                                            |
| N6, Fil d'événements                            | `EventFeed`, `EventLine`                            |

The fonts (OFL, with their licenses), the sparkles and the icons are in `src/assets/`, served by the panel:
its CSP takes nothing from another host.

## Develop

Node LTS. From this folder:

```sh
npm ci
npm run dev
```

`npm run dev` serves the panel on <http://127.0.0.1:5173> and forwards `/api/` to a master on
`http://127.0.0.1:8080`, or the address in `RTYPE_MASTER`:

```sh
RTYPE_MASTER=http://127.0.0.1:8081 npm run dev
```

The session comes from the master: open the address of its `admin-url.txt` once, then the development server on
the same host name (`127.0.0.1`, not `localhost`): a cookie is shared by every port of a host.

## Build

```sh
npm run build        # dist/
```

To serve it from a master, give `dist/` to the server: `panel_dir = "…/web/admin/dist"` in its
[settings](../../docs/guides/configure-a-server.md), or install it beside the server with CMake:

```sh
cmake -S . -B build -DRTYPE_ADMIN_PANEL_DIR=$PWD/web/admin/dist
cmake --install build --component server --prefix /opt/r-type     # /opt/r-type/admin
```

## Check and test

```sh
npm run lint            # ESLint, no warning allowed
npm run format:check    # Prettier; npm run format fixes
npm run typecheck
npm test                # Vitest: the store, the client of the API, the components and the pages, the API mocked
npm run test:e2e        # Playwright: a real master and a game of real players
```

The end-to-end tests need the panel built (`npm run build`) and the server built with its tests: they start
`r-type_server` and `rtype_panel_players` from `../../build/bin`, or from `RTYPE_BUILD_BIN`, and Chromium
(`npx playwright install chromium`, once). Linux only.

## Layout

```
src/
  api/          the client of the API (its CSRF token) and its types
  live/         the WebSocket of /api/events: its store, reconnected, and the state it keeps
  theme/        the Chakra system after the Figma design, and the fonts
  components/   the elements of the design, the dialogs of the actions
  pages/        Overview, Games, a game, Players, Actions
  strings.ts    every text, in English, in one place
e2e/            Playwright, and the master it starts
```
