-- The ship that picks it up cannot be hit for a while.
return {
    id = "shield",
    model = "models/items/pu_immunity.glb",
    collider = { shape = "sphere", radius = 30 },
    effects = {
        -- Seconds it cannot be hit
        { type = "shield", duration = 6 },
    },
    lifetime = 10,
    speed = 60,
}
