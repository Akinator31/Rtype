-- Gives back health to the ship that picks it up. Fields: docs/guides/configure-enemies-and-weapons.md
return {
    id = "repair_kit",
    -- The 3D model the client draws, relative to its assets: its "Idle" animation loops, its "Collect" one
    -- plays where it is picked up
    model = "models/items/pu_heal.glb",
    -- Its hitbox: the ships that touch it pick it up. The model is drawn at its size
    collider = { shape = "sphere", radius = 30 },
    -- What it does, in order: heal, extra_life, damage, rapid_fire, shield
    effects = {
        -- Hit points given back, up to the health of the player
        { type = "heal", amount = 40 },
    },
    -- Seconds before it vanishes if nobody picks it up (default 10)
    lifetime = 10,
    -- Speed it drifts to the left at, in game units per second (default 60)
    speed = 60,
}
