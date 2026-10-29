# Contributing to the panel

The administration panel, `web/admin/`, is the one part of the repository not in C++: TypeScript, built by Node
([ADR 0025](../decisions/0025-server-administration.md)). Building the game and the server needs no Node; a
server without its panel says it is not built. How the panel works: [The panel](../architecture/panel.md).

## Toolchain

| | |
|---|---|
| Runtime | Node LTS, npm; the versions of the dependencies pinned by `web/admin/package-lock.json` |
| Language | TypeScript, `strict`, with `noUncheckedIndexedAccess` |
| Lint | ESLint (`typescript-eslint` strict, React Hooks, React Refresh), no warning allowed |
| Format | Prettier, 110 columns, single quotes (`.prettierrc.json`) |
| Tests | Vitest, Testing Library and MSW; Playwright and Chromium for the end-to-end tests |

```sh
cd web/admin
npm ci
npm run dev             # the panel, /api/ forwarded to a master on 127.0.0.1:8080 (RTYPE_MASTER)
npm run lint
npm run format          # or format:check
npm run typecheck
npm test
npm run build           # dist/
npm run test:e2e        # after the build, and the server built with its tests
```

The details: [web/admin/README.md](https://github.com/Akinator31/Rtype/blob/main/web/admin/README.md).

## Rules

- **The design is the Figma frame** [R-Type UI — Éléments retenus](https://www.figma.com/design/RrrH0ic4ttdyDoc3nRnWBr/Untitled?node-id=42-3).
  A colour, a font, a size or an effect of the design lives in `src/theme/system.ts`, once; pages use its tokens,
  text styles and recipes, or the components of the design, never a value of their own. No colour but black,
  silver and the red of the alerts; no filled button.
- **Every text in `src/strings.ts`**, in English: a French panel replaces that object.
- **Every action behind a dialog**, which confirms it and shows the error of the master.
- **Nothing from another host**: fonts and images are files of the panel, no inline script, no `eval`. The CSP
  of the master refuses anything else ([Within the CSP](../architecture/panel.md#within-the-csp)), and the
  end-to-end tests fail on it.
- **New behaviour comes with tests**: the pure parts alone (`src/live/state.ts`), the pages with
  Testing Library and the API mocked by MSW (`src/test/server.ts`), what crosses the master in `e2e/`.
- The panel follows the [administration API](../protocol/admin-api.md): a change of the API changes
  `src/api/types.ts` in the same pull request.

## Pull requests

A pull request that changes the panel shows **screenshots of the pages it changes, beside the matching frame of
the Figma design**.

The *Build* workflow runs a *Panel* job first, on the Linux runner: install, lint, format, types, tests, build.
The build jobs of both systems then put its `dist/` in the server package, as `admin/`; the Linux job also runs
the end-to-end tests against the server it built ([Continuous integration](testing.md#continuous-integration)).
