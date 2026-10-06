# 0005 — ECS component storage

| | |
|---|---|
| **Status** | Accepted |
| **Date** | 2026-10-06 |

## Context

The game engine is built around an Entity Component System (ECS): entities are only ids, components are
plain data attached to them, and systems run the logic on the entities that have the components they need.
The way components are stored decides how fast systems reach them, and how simple the engine is to write,
debug and keep deterministic between the client and the server (prediction, reconciliation).

The expected load is a few hundred entities, with common components (position, velocity, sprite) and
entities destroyed in the middle of a system update (a missile hits, an entity leaves the screen).

## Options

### Storage of the components of one type

| | Indexed vector | Sparse set | Hash map |
|---|---|---|---|
| Structure | `vector<optional<T>>`, index = entity id | Dense array of components + `sparse` array of indices + entity array | `unordered_map<entity, T>` |
| Access to a component | 1 memory access | 2 memory accesses | Hash + pointer chasing |
| Iteration | Visits every slot, skips empty ones | Contiguous, only existing components | Scattered nodes, poor locality |
| Memory | `N × sizeof(optional<T>)`, wasted if the component is rare | Compact if rare, larger than the vector if almost every entity has it | Compact but one allocation per component |
| Multi-component join | Same index in every cache, trivial | One sequential access + one random access | One lookup per entity |
| Destroying an entity while iterating | Safe, the slot is reset | Swap-and-pop breaks the forward iteration | Invalidates iterators on rehash |
| Order of iteration | Entity id order, deterministic | Depends on the add/remove history | Depends on hash and rehash |
| Code | ~40 lines | ~100 lines | Short |

The sparse set is to be preferred with tens of thousands of entities, components carried by less than
about 10-20 % of them, large rare components, or many temporary tags. None of these applies yet.

### Finding the cache of a type

A hash map `type_index → cache` is fine here: there are few types, and the lookup is done once per system,
never per entity.

### Per-system list of matching entities

Each system could keep the list of entities that have all its components, updated when a component is added
or removed. It avoids testing every slot at each frame, but requires component signatures, keeping the lists
in sync, and deferring the destruction of entities during an update. Not worth it at this scale.

## Decision

- **Indexed vector** for each component cache (`ComponentCache<T>`), behind a small interface (`emplace`,
  `erase`, `has`, `get`, `size`) so that a type can later move to a sparse set without touching the systems.
- **Hash map `type_index → cache`** in `ComponentCaches`, with a type-erased base (`ICache`) to remove an
  entity from every cache when it is destroyed.
- **Systems** keep a direct reference to each cache they use, obtained once in their constructor with
  `use<T>()`, which also records the component type in the system (`componentTypes()`). Their `update()`
  walks the entity ids and tests `has()` on its caches.
- **Entity ids** are `uint32_t`, recycled through a free list so the caches stay bounded.
- No per-system entity list for now.

## Consequences

- A component is reached with a direct memory access, and systems can destroy entities while iterating.
- Iteration order is the entity id order, the same on the client and the server.
- Memory is wasted for rare components and every system visits all slots up to the highest entity id.
- Adding a component of an iterated type from inside a system may reallocate its cache and invalidate
  references; this must be avoided.
- If profiling shows a problem, the per-system entity list or a sparse set for the faulty types can be added
  without changing the public interface of the caches. A new record would then supersede this one.
