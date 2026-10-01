# 0003 — Zensical and Doxygen for the documentation

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-01 |

## Context

The documentation must be in English, browsable online with navigation and search, and go beyond generated
API docs: architecture, protocol, decisions, tutorials. Adding a page must be easy for every team member,
and documenting the engine API later must not require changing the tooling.

## Options

| | Zensical | Material for MkDocs | VitePress | Docusaurus | Sphinx |
|---|---|---|---|---|---|
| Source format | Markdown | Markdown | Markdown | Markdown / MDX | reStructuredText / MyST |
| Look out of the box | Very good | Very good | Good | Good | Plain (theme dependent) |
| Toolchain | Python | Python | Node | Node (React) | Python |
| Maintenance | Active (successor of Material) | End of life on 2026-11-05 | Active | Active | Active |
| C++ API docs | Via Doxygen | Via Doxygen | Via Doxygen | Via Doxygen | Breathe + Doxygen |

## Decision

- **Zensical** builds the documentation site from Markdown files in `docs/`. It is made by the authors of
  Material for MkDocs, which reaches end of life in November 2026, and reads the same configuration format.
- **Doxygen**, with the doxygen-awesome-css theme, generates the C++ API reference from source comments and
  is published under `/api/` on the same site.
- Both are built by the *Documentation* workflow and deployed to GitHub Pages.

## Consequences

- Zensical is still pre-1.0: its version is pinned in `requirements-docs.txt` and updated deliberately.
- The navigation is listed explicitly in `zensical.toml`; `scripts/check_docs_nav.py` fails the CI when a
  page is missing from it.
- Doxygen scans the whole repository, so new source folders appear in the API reference without any
  configuration change.
